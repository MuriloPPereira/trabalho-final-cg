#include "entities/CamouflagedPursuer.h"

#include "engine/Renderer.h"
#include "engine/Shader.h"
#include "entities/Player.h"
#include "matrices.h"
#include "utils/Bezier.h"
#include "utils/Constants.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec2.hpp>

namespace {
const int kPathSegmentCount = 4;
const int kBezierLengthSamples = 24;
const int kPathJoinLookAheadSamples = 8;
const float kPathJoinTolerance = 0.01f;
const float kPathJoinLookAheadDistance = 2.0f;
const float kMaxPursuerChaseDeltaTime = 1.0f / 30.0f;

struct PursuerPathSegment {
  bool cubic;
  glm::vec2 p0;
  glm::vec2 p1;
  glm::vec2 p2;
  glm::vec2 p3;
  float length;
};

struct PursuerPath {
  std::array<PursuerPathSegment, kPathSegmentCount> segments;
  std::array<float, kPathSegmentCount + 1> cumulativeLengths;
  glm::vec2 blockOffset;
  float length;
};

struct PathLocation {
  glm::vec2 position;
  glm::vec2 tangent;
  float progress;
  int section;
};

float Clamp01(float value) {
  return std::max(0.0f, std::min(value, 1.0f));
}

float CubicArcLength(const PursuerPathSegment &segment, float end_t) {
  const int sample_count = std::max(
      1, static_cast<int>(std::ceil(kBezierLengthSamples * Clamp01(end_t))));
  glm::vec2 previous = segment.p0;
  float length = 0.0f;
  for (int i = 1; i <= sample_count; ++i) {
    const float t = end_t * static_cast<float>(i) /
                    static_cast<float>(sample_count);
    const glm::vec2 current = EvaluateCubicBezier(
        t, segment.p0, segment.p1, segment.p2, segment.p3);
    length += glm::length(current - previous);
    previous = current;
  }
  return length;
}

float SegmentLength(const PursuerPathSegment &segment, float end_t) {
  if (segment.cubic)
    return CubicArcLength(segment, end_t);
  return segment.length * Clamp01(end_t);
}

PursuerPath BuildPursuerPath() {
  const CanonicalCorridorLayout layout = GetCanonicalCorridorLayout();
  const float turn_radius = 0.5f * kCornerLength;
  const float control_distance = turn_radius * 0.55228475f;

  PursuerPath path;
  path.blockOffset = layout.block_offset;
  path.segments[0] = {false,
                      glm::vec2(0.0f, kCorridorZ0),
                      glm::vec2(0.0f),
                      glm::vec2(0.0f),
                      glm::vec2(0.0f, layout.turn_z0),
                      kCorridorLength};
  path.segments[1] = {
      true,
      glm::vec2(0.0f, layout.turn_z0),
      glm::vec2(0.0f, layout.turn_z0 - control_distance),
      glm::vec2(layout.connector_start_x + control_distance,
                layout.connector_center_z),
      glm::vec2(layout.connector_start_x, layout.connector_center_z),
      0.0f};
  path.segments[2] = {false,
                      glm::vec2(layout.connector_start_x,
                                layout.connector_center_z),
                      glm::vec2(0.0f),
                      glm::vec2(0.0f),
                      glm::vec2(layout.connector_end_x,
                                layout.connector_center_z),
                      layout.connector_length};
  path.segments[3] = {
      true,
      glm::vec2(layout.connector_end_x, layout.connector_center_z),
      glm::vec2(layout.connector_end_x - control_distance,
                layout.connector_center_z),
      glm::vec2(layout.exit_turn_x,
                layout.second_corridor_z_offset + control_distance),
      glm::vec2(layout.exit_turn_x, layout.second_corridor_z_offset),
      0.0f};

  path.cumulativeLengths[0] = 0.0f;
  for (int i = 0; i < kPathSegmentCount; ++i) {
    PursuerPathSegment &segment = path.segments[i];
    if (segment.cubic)
      segment.length = CubicArcLength(segment, 1.0f);
    path.cumulativeLengths[i + 1] =
        path.cumulativeLengths[i] + segment.length;
  }
  path.length = path.cumulativeLengths[kPathSegmentCount];
  return path;
}

glm::vec2 EvaluateSegment(const PursuerPathSegment &segment, float t) {
  t = Clamp01(t);
  if (segment.cubic)
    return EvaluateCubicBezier(t, segment.p0, segment.p1, segment.p2,
                               segment.p3);
  return segment.p0 + t * (segment.p3 - segment.p0);
}

glm::vec2 EvaluateSegmentTangent(const PursuerPathSegment &segment, float t) {
  glm::vec2 tangent = segment.cubic
                          ? EvaluateCubicBezierDerivative(
                                Clamp01(t), segment.p0, segment.p1, segment.p2,
                                segment.p3)
                          : segment.p3 - segment.p0;
  if (glm::length(tangent) < 0.0001f)
    return glm::vec2(0.0f, -1.0f);
  return glm::normalize(tangent);
}

float SegmentTForDistance(const PursuerPathSegment &segment, float distance) {
  if (!segment.cubic || segment.length < 0.0001f)
    return Clamp01(distance / std::max(segment.length, 0.0001f));

  float low = 0.0f;
  float high = 1.0f;
  for (int i = 0; i < 12; ++i) {
    const float mid = 0.5f * (low + high);
    if (CubicArcLength(segment, mid) < distance)
      low = mid;
    else
      high = mid;
  }
  return 0.5f * (low + high);
}

PathLocation EvaluatePath(const PursuerPath &path, float progress) {
  const int block = static_cast<int>(std::floor(progress / path.length));
  float local_progress = progress - static_cast<float>(block) * path.length;
  if (local_progress < 0.0f)
    local_progress += path.length;

  int section = kPathSegmentCount - 1;
  for (int i = 0; i < kPathSegmentCount; ++i) {
    if (local_progress <= path.cumulativeLengths[i + 1]) {
      section = i;
      break;
    }
  }

  const PursuerPathSegment &segment = path.segments[section];
  const float segment_distance =
      local_progress - path.cumulativeLengths[section];
  const float t = SegmentTForDistance(segment, segment_distance);
  PathLocation location;
  location.position = EvaluateSegment(segment, t) +
                      static_cast<float>(block) * path.blockOffset;
  location.tangent = EvaluateSegmentTangent(segment, t);
  location.progress = progress;
  location.section = section;
  return location;
}

float ClosestSegmentT(const PursuerPathSegment &segment,
                      const glm::vec2 &point) {
  if (!segment.cubic) {
    const glm::vec2 delta = segment.p3 - segment.p0;
    const float length_squared = glm::dot(delta, delta);
    if (length_squared < 0.0001f)
      return 0.0f;
    return Clamp01(glm::dot(point - segment.p0, delta) / length_squared);
  }

  float best_t = 0.0f;
  float best_distance_squared = glm::dot(point - segment.p0,
                                         point - segment.p0);
  for (int i = 1; i <= kBezierLengthSamples; ++i) {
    const float t = static_cast<float>(i) / kBezierLengthSamples;
    const glm::vec2 difference = EvaluateSegment(segment, t) - point;
    const float distance_squared = glm::dot(difference, difference);
    if (distance_squared < best_distance_squared) {
      best_distance_squared = distance_squared;
      best_t = t;
    }
  }

  float low = std::max(0.0f, best_t - 1.0f / kBezierLengthSamples);
  float high = std::min(1.0f, best_t + 1.0f / kBezierLengthSamples);
  for (int i = 0; i < 12; ++i) {
    const float left = (2.0f * low + high) / 3.0f;
    const float right = (low + 2.0f * high) / 3.0f;
    const glm::vec2 left_difference = EvaluateSegment(segment, left) - point;
    const glm::vec2 right_difference = EvaluateSegment(segment, right) - point;
    if (glm::dot(left_difference, left_difference) <
        glm::dot(right_difference, right_difference))
      high = right;
    else
      low = left;
  }
  return 0.5f * (low + high);
}

PathLocation ProjectOntoPath(const PursuerPath &path,
                             const glm::vec2 &world_point) {
  const float offset_length_squared = glm::dot(path.blockOffset,
                                                path.blockOffset);
  const int estimated_block = static_cast<int>(std::floor(
      glm::dot(world_point, path.blockOffset) / offset_length_squared +
      0.5f));

  PathLocation best = EvaluatePath(path, 0.0f);
  float best_distance_squared =
      glm::dot(best.position - world_point, best.position - world_point);
  for (int block = estimated_block - 2; block <= estimated_block + 2;
       ++block) {
    const glm::vec2 local_point =
        world_point - static_cast<float>(block) * path.blockOffset;
    for (int section = 0; section < kPathSegmentCount; ++section) {
      const PursuerPathSegment &segment = path.segments[section];
      const float t = ClosestSegmentT(segment, local_point);
      const glm::vec2 candidate =
          EvaluateSegment(segment, t) +
          static_cast<float>(block) * path.blockOffset;
      const glm::vec2 difference = candidate - world_point;
      const float distance_squared = glm::dot(difference, difference);
      if (distance_squared < best_distance_squared) {
        best_distance_squared = distance_squared;
        best.position = candidate;
        best.tangent = EvaluateSegmentTangent(segment, t);
        best.progress = static_cast<float>(block) * path.length +
                        path.cumulativeLengths[section] +
                        SegmentLength(segment, t);
        best.section = section;
      }
    }
  }
  return best;
}

bool InsideBox(const WalkableBox2D &box, const glm::vec2 &point) {
  return point.x >= box.min_x && point.x <= box.max_x &&
         point.y >= box.min_z && point.y <= box.max_z;
}

bool ShareWalkableSection(const PursuerPath &path, const glm::vec2 &a,
                          const glm::vec2 &b) {
  const CanonicalCorridorLayout layout = GetCanonicalCorridorLayout();
  const std::array<WalkableBox2D, kCorridorWalkableSectionCount>
      local_sections = GetCorridorWalkableSections(
          layout, kCorridorHalfWidth, kCorridorZ1, 0.0f);

  const float offset_length_squared = glm::dot(path.blockOffset,
                                                path.blockOffset);
  const glm::vec2 midpoint = 0.5f * (a + b);
  const int estimated_block = static_cast<int>(std::floor(
      glm::dot(midpoint, path.blockOffset) / offset_length_squared + 0.5f));
  for (int block = estimated_block - 2; block <= estimated_block + 2;
       ++block) {
    const glm::vec2 offset = static_cast<float>(block) * path.blockOffset;
    const glm::vec2 local_a = a - offset;
    const glm::vec2 local_b = b - offset;
    for (const WalkableBox2D &section : local_sections) {
      if (InsideBox(section, local_a) && InsideBox(section, local_b))
        return true;
    }
  }
  return false;
}

PathLocation ChoosePathJoinLocation(
    const PursuerPath &path, const glm::vec2 &pursuer_position,
    const PathLocation &nearest_path_location,
    const PathLocation &player_path_location) {
  const float path_difference =
      player_path_location.progress - nearest_path_location.progress;
  const float direction = (path_difference < 0.0f) ? -1.0f : 1.0f;
  const float look_ahead_distance = std::min(
      std::abs(path_difference), kPathJoinLookAheadDistance);
  const glm::vec2 to_player =
      player_path_location.position - pursuer_position;

  PathLocation join_location = nearest_path_location;
  float best_alignment = glm::dot(
      nearest_path_location.position - pursuer_position, to_player);
  for (int sample = 1; sample <= kPathJoinLookAheadSamples; ++sample) {
    const float sample_distance =
        look_ahead_distance * static_cast<float>(sample) /
        static_cast<float>(kPathJoinLookAheadSamples);
    const PathLocation candidate = EvaluatePath(
        path, nearest_path_location.progress + direction * sample_distance);
    if (!ShareWalkableSection(path, pursuer_position, candidate.position))
      break;
    const float alignment =
        glm::dot(candidate.position - pursuer_position, to_player);
    if (alignment > best_alignment) {
      best_alignment = alignment;
      join_location = candidate;
    }
  }
  return join_location;
}

float MoveTowards(glm::vec3 &position, const glm::vec3 &target,
                  float movement_budget, glm::vec3 &forward) {
  glm::vec3 difference = target - position;
  difference.y = 0.0f;
  const float distance = glm::length(difference);
  if (distance < 0.0001f)
    return movement_budget;
  forward = difference / distance;
  const float movement = std::min(movement_budget, distance);
  position += forward * movement;
  return movement_budget - movement;
}
} // namespace

CamouflagedPursuerState g_CamouflagedPursuer;
SalarymanAnimatedModel g_CamouflagedPursuerAnimatedModel;
SalarymanAnimator g_CamouflagedPursuerAnimator;

CamouflagedPursuerState::CamouflagedPursuerState()
    : active(false), visible(false), chasing(false), corridorId(-1),
      triggerRadius(kCamouflagedPursuerTriggerRadius), movementSpeed(0.0f),
      position(0.0f), forward(0.0f, 0.0f, -1.0f), pathSection(-1),
      pathProgress(0.0f), followingPath(false), useAnimation(false),
      placeholderModel(NULL), animatedModel(NULL), animator(NULL) {}

void ResetCamouflagedPursuer(CamouflagedPursuerState &pursuer) {
  pursuer.active = false;
  pursuer.visible = false;
  pursuer.chasing = false;
  pursuer.corridorId = -1;
  pursuer.triggerRadius = kCamouflagedPursuerTriggerRadius;
  pursuer.movementSpeed = 0.0f;
  pursuer.position = glm::vec3(0.0f);
  pursuer.forward = glm::vec3(0.0f, 0.0f, -1.0f);
  pursuer.pathSection = -1;
  pursuer.pathProgress = 0.0f;
  pursuer.followingPath = false;
  if (pursuer.animator != NULL)
    pursuer.animator->currentTime = 0.0f;
  if (pursuer.animatedModel != NULL && pursuer.animatedModel->loaded)
    SetAnimatedModelToBindPose(*pursuer.animatedModel);
}

void ActivateCamouflagedPursuerForCorridor(
    CamouflagedPursuerState &pursuer, const CorridorContent &content) {
  ResetCamouflagedPursuer(pursuer);
  if (content.anomalyType != kCorridorAnomalyCamouflagedPursuer)
    return;

  pursuer.active = true;
  pursuer.visible = true;
  pursuer.corridorId = content.corridorId;
  pursuer.triggerRadius = kCamouflagedPursuerTriggerRadius;
  pursuer.movementSpeed = GetPlayerThirdPersonShiftSprintSpeed();
  pursuer.position = content.camouflagedPursuerSpawnPosition;
  pursuer.forward = -content.frame.contentForward;
}

void UpdateCamouflagedPursuer(CamouflagedPursuerState &pursuer,
                              float delta_time,
                              const glm::vec3 &player_position) {
  if (!pursuer.active)
    return;

  glm::vec3 to_player = player_position - pursuer.position;
  to_player.y = 0.0f;
  const float distance_to_player = glm::length(to_player);
  if (!pursuer.chasing) {
    if (distance_to_player > pursuer.triggerRadius)
      return;
    pursuer.chasing = true;
    if (pursuer.animator != NULL)
      pursuer.animator->currentTime = 0.0f;
  }

  if (distance_to_player <= kCamouflagedPursuerStopDistance)
    return;

  pursuer.movementSpeed = GetPlayerThirdPersonShiftSprintSpeed();
  const float chase_delta_time =
      std::max(0.0f, std::min(delta_time, kMaxPursuerChaseDeltaTime));
  float movement_budget = pursuer.movementSpeed * chase_delta_time;
  const PursuerPath path = BuildPursuerPath();
  const glm::vec2 pursuer_ground(pursuer.position.x, pursuer.position.z);
  const glm::vec2 player_ground(player_position.x, player_position.z);

  if (ShareWalkableSection(path, pursuer_ground, player_ground)) {
    const float remaining_distance =
        distance_to_player - kCamouflagedPursuerStopDistance;
    const float movement_distance =
        std::min(movement_budget, remaining_distance);
    pursuer.forward = to_player / distance_to_player;
    pursuer.position += pursuer.forward * movement_distance;
    pursuer.followingPath = false;
  } else {
    const PathLocation player_path_location =
        ProjectOntoPath(path, player_ground);
    if (!pursuer.followingPath) {
      const PathLocation nearest_path_location =
          ProjectOntoPath(path, pursuer_ground);
      const PathLocation join_location = ChoosePathJoinLocation(
          path, pursuer_ground, nearest_path_location, player_path_location);
      pursuer.pathProgress = join_location.progress;
      pursuer.pathSection = join_location.section;
      pursuer.followingPath = true;
    }

    PathLocation path_location = EvaluatePath(path, pursuer.pathProgress);
    const glm::vec3 join_target(path_location.position.x, pursuer.position.y,
                                path_location.position.y);
    if (glm::length(join_target - pursuer.position) > kPathJoinTolerance)
      movement_budget = MoveTowards(pursuer.position, join_target,
                                    movement_budget, pursuer.forward);

    if (movement_budget > 0.0f &&
        glm::length(join_target - pursuer.position) <= kPathJoinTolerance) {
      const float path_difference =
          player_path_location.progress - pursuer.pathProgress;
      const float direction = (path_difference < 0.0f) ? -1.0f : 1.0f;
      const float path_movement =
          std::min(movement_budget, std::abs(path_difference));
      pursuer.pathProgress += direction * path_movement;
      path_location = EvaluatePath(path, pursuer.pathProgress);
      const glm::vec3 old_position = pursuer.position;
      pursuer.position.x = path_location.position.x;
      pursuer.position.z = path_location.position.y;
      glm::vec3 path_delta = pursuer.position - old_position;
      path_delta.y = 0.0f;
      if (glm::length(path_delta) > 0.0001f)
        pursuer.forward = glm::normalize(path_delta);
      else
        pursuer.forward = glm::vec3(direction * path_location.tangent.x, 0.0f,
                                    direction * path_location.tangent.y);
      pursuer.pathSection = path_location.section;
    }
  }
  if (pursuer.useAnimation && pursuer.animator != NULL)
    UpdateSalarymanAnimation(*pursuer.animator, delta_time);
}

bool HasCamouflagedPursuerCaughtPlayer(
    const CamouflagedPursuerState &pursuer,
    const glm::vec3 &player_position) {
  if (!pursuer.active || !pursuer.visible || !pursuer.chasing)
    return false;

  glm::vec3 to_player = player_position - pursuer.position;
  to_player.y = 0.0f;
  return glm::length(to_player) <=
         kCamouflagedPursuerStopDistance + 0.001f;
}

void DrawCamouflagedPursuer(const CamouflagedPursuerState &pursuer,
                            const Material &material) {
  const bool has_animated_model =
      pursuer.useAnimation && pursuer.animatedModel != NULL &&
      pursuer.animatedModel->loaded;
  const bool has_placeholder = pursuer.placeholderModel != NULL &&
                               pursuer.placeholderModel->loaded;
  if (!pursuer.active || !pursuer.visible ||
      (!has_animated_model && !has_placeholder))
    return;

  glm::vec3 forward = pursuer.forward;
  if (glm::length(forward) < 0.0001f)
    forward = glm::vec3(0.0f, 0.0f, -1.0f);
  else
    forward = glm::normalize(forward);

  const glm::vec3 up(0.0f, 1.0f, 0.0f);
  glm::vec3 right = glm::cross(up, forward);
  if (glm::length(right) < 0.0001f)
    right = glm::vec3(1.0f, 0.0f, 0.0f);
  else
    right = glm::normalize(right);
  const glm::vec3 corrected_up = glm::normalize(glm::cross(forward, right));
  glm::vec3 p = pursuer.position;
  if (!pursuer.chasing)
    p -= forward * kCamouflagedPursuerIdleWallEmbedDepth;
  const glm::mat4 model_matrix =
      Matrix(right.x, corrected_up.x, forward.x, p.x, right.y, corrected_up.y,
             forward.y, p.y, right.z, corrected_up.z, forward.z, p.z, 0.0f,
             0.0f, 0.0f, 1.0f);

  ApplyMaterial(material);
  glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                     glm::value_ptr(model_matrix));

  if (has_animated_model) {
    const glm::mat4 animated_model_matrix =
        model_matrix * pursuer.animatedModel->normalizationMatrix;
    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                       glm::value_ptr(animated_model_matrix));
    DrawAnimatedModel(*pursuer.animatedModel);
  } else {
    DrawStaticModel(*pursuer.placeholderModel);
  }
}
