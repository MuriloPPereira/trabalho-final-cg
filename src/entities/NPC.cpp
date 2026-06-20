#include "entities/NPC.h"

#include "engine/Renderer.h"
#include "engine/Shader.h"
#include "matrices.h"
#include "utils/Bezier.h"
#include "utils/Constants.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec2.hpp>

namespace {
const int kSalarymanPathSegmentCount = 4;
const int kBezierLengthSamples = 24;

struct SalarymanPathSegment {
  bool cubic;
  glm::vec2 p0;
  glm::vec2 p1;
  glm::vec2 p2;
  glm::vec2 p3;
  float length;
};

struct SalarymanPath {
  std::array<SalarymanPathSegment, kSalarymanPathSegmentCount> segments;
  std::array<float, kSalarymanPathSegmentCount + 1> cumulativeLengths;
  glm::vec2 blockOffset;
  float length;
};

float Clamp01(float value) {
  return std::max(0.0f, std::min(value, 1.0f));
}

glm::vec2 EvaluatePathSegment(const SalarymanPathSegment &segment, float t) {
  t = Clamp01(t);
  if (segment.cubic)
    return EvaluateCubicBezier(t, segment.p0, segment.p1, segment.p2,
                               segment.p3);
  return segment.p0 + t * (segment.p3 - segment.p0);
}

float CubicArcLength(const SalarymanPathSegment &segment, float end_t) {
  const int sample_count = std::max(
      1, static_cast<int>(std::ceil(kBezierLengthSamples * Clamp01(end_t))));
  glm::vec2 previous = segment.p0;
  float length = 0.0f;
  for (int i = 1; i <= sample_count; ++i) {
    const float t = end_t * static_cast<float>(i) /
                    static_cast<float>(sample_count);
    const glm::vec2 current = EvaluatePathSegment(segment, t);
    length += glm::length(current - previous);
    previous = current;
  }
  return length;
}

SalarymanPath BuildSalarymanPath() {
  const CanonicalCorridorLayout layout = GetCanonicalCorridorLayout();
  const float turn_radius = 0.5f * kCornerLength;
  const float control_distance = turn_radius * 0.55228475f;

  SalarymanPath path;
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
  for (int i = 0; i < kSalarymanPathSegmentCount; ++i) {
    SalarymanPathSegment &segment = path.segments[i];
    if (segment.cubic)
      segment.length = CubicArcLength(segment, 1.0f);
    path.cumulativeLengths[i + 1] =
        path.cumulativeLengths[i] + segment.length;
  }
  path.length = path.cumulativeLengths[kSalarymanPathSegmentCount];
  return path;
}

const SalarymanPath &GetSalarymanPath() {
  static const SalarymanPath path = BuildSalarymanPath();
  return path;
}

float SegmentTForDistance(const SalarymanPathSegment &segment,
                          float distance) {
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

glm::vec3 EvaluateSalarymanPath(const SalarymanPath &path, float progress) {
  const int block = static_cast<int>(std::floor(progress / path.length));
  float local_progress = progress - static_cast<float>(block) * path.length;
  if (local_progress < 0.0f)
    local_progress += path.length;

  int section = kSalarymanPathSegmentCount - 1;
  for (int i = 0; i < kSalarymanPathSegmentCount; ++i) {
    if (local_progress <= path.cumulativeLengths[i + 1]) {
      section = i;
      break;
    }
  }

  const SalarymanPathSegment &segment = path.segments[section];
  const float segment_distance =
      local_progress - path.cumulativeLengths[section];
  const float t = SegmentTForDistance(segment, segment_distance);
  const glm::vec2 position =
      EvaluatePathSegment(segment, t) +
      static_cast<float>(block) * path.blockOffset;
  return glm::vec3(position.x, 0.0f, position.y);
}

float ProjectConnectorSpawnOntoPath(const SalarymanPath &path,
                                    const glm::vec3 &spawn_position) {
  const int connector_section = 2;
  const SalarymanPathSegment &connector = path.segments[connector_section];
  const glm::vec2 delta = connector.p3 - connector.p0;
  const glm::vec2 spawn(spawn_position.x, spawn_position.z);
  const float length_squared = glm::dot(delta, delta);
  if (length_squared < 0.0001f)
    return 0.0f;

  float best_progress = 0.0f;
  float best_distance_squared = -1.0f;
  for (int block = -1; block <= 0; ++block) {
    const glm::vec2 offset = static_cast<float>(block) * path.blockOffset;
    const float t = Clamp01(
        glm::dot(spawn - offset - connector.p0, delta) / length_squared);
    const glm::vec2 candidate = connector.p0 + t * delta + offset;
    const glm::vec2 difference = candidate - spawn;
    const float distance_squared = glm::dot(difference, difference);
    if (best_distance_squared < 0.0f ||
        distance_squared < best_distance_squared) {
      best_distance_squared = distance_squared;
      best_progress = static_cast<float>(block) * path.length +
                      path.cumulativeLengths[connector_section] +
                      t * connector.length;
    }
  }
  return best_progress;
}

float LocalPathProgress(const SalarymanPath &path, float progress) {
  float local_progress = std::fmod(progress, path.length);
  if (local_progress < 0.0f)
    local_progress += path.length;
  return local_progress;
}
} // namespace

StaticModel g_SalarymanStaticModel;
SalarymanAnimatedModel g_SalarymanAnimatedModel;
SalarymanAnimator g_SalarymanAnimator;
SalarymanNPC g_SalarymanNPC;

SalarymanNPC::SalarymanNPC()
    : active(false), corridorId(-1), position(0.0f, 0.0f, 0.0f),
      forward(0.0f, 0.0f, -1.0f), speed(2.5f), corridorLength(0.0f),
      corridorOrigin(0.0f, 0.0f, 0.0f), useAnimation(false), useBezier(false),
      reverseBezier(false), bezierT(0.0f), pathStartProgress(0.0f),
      pathTravelDistance(0.0f), spawnGraceTimer(0.0f), p0(0.0f), p1(0.0f),
      p2(0.0f), p3(0.0f), model(NULL), animatedModel(NULL), animator(NULL),
      isGiant(false) {}

void SpawnSalarymanForCorridor(SalarymanNPC &salaryman,
                               const CorridorContent &content,
                               const glm::vec3 &player_position) {
  const CorridorContentFrame &frame = content.frame;
  glm::vec3 path_forward = content.salarymanForward;
  if (glm::length(path_forward) < 0.0001f)
    path_forward = frame.contentForward;
  if (glm::length(path_forward) < 0.0001f)
    path_forward = glm::vec3(0.0f, 0.0f, -1.0f);
  else
    path_forward = glm::normalize(path_forward);

  const glm::vec3 spawn_position = content.salarymanSpawnPosition;
  salaryman.active = true;
  salaryman.corridorId = frame.logicalCorridorId;
  salaryman.isGiant = (content.anomalyType == kCorridorAnomalyGiantNPC);
  // A velocidade agora puxa do construtor, ou pode ser configurada dinamicamente
  salaryman.corridorLength = frame.corridorLength;
  salaryman.corridorOrigin =
      frame.contentOrigin - path_forward * frame.corridorLength;

  const SalarymanPath &path = GetSalarymanPath();
  const glm::vec3 canonical_forward(0.0f, 0.0f, -1.0f);

  // Follow the same canonical centerline used by the corridor and connector.
  // The generated content decides both where this NPC starts and which way it
  // walks; the path only bends that movement through valid turns.
  salaryman.useBezier = true;
  salaryman.reverseBezier = glm::dot(path_forward, canonical_forward) < 0.0f;
  salaryman.bezierT = 0.0f;
  salaryman.pathStartProgress =
      ProjectConnectorSpawnOntoPath(path, spawn_position);
  const float local_start =
      LocalPathProgress(path, salaryman.pathStartProgress);
  const float connector_inset = salaryman.reverseBezier
                                    ? local_start - path.cumulativeLengths[2]
                                    : path.cumulativeLengths[3] - local_start;
  salaryman.pathTravelDistance =
      path.length - frame.connectorLength +
      2.0f * std::max(0.0f, connector_inset);
  salaryman.position = spawn_position;
  salaryman.forward = path_forward;
  // Grace period: skip distance-based despawn for the first 2 seconds after
  // spawn, because the teleport coordinate wrapping can place the camera far
  // from the NPC's initial position in canonical space.
  salaryman.spawnGraceTimer = 2.0f;

  salaryman.useAnimation =
      (salaryman.animatedModel != NULL && salaryman.animatedModel->loaded &&
       salaryman.animator != NULL);
  if (salaryman.useAnimation) {
    salaryman.animator->currentTime = 0.0f;
    UpdateSalarymanAnimation(*salaryman.animator, 0.0f);
  }

  (void)player_position;
}

void UpdateSalarymanNPC(SalarymanNPC &salaryman, float delta_time,
                        const glm::vec4 &camera_position_c) {
  if (!salaryman.active)
    return;

  if (salaryman.useAnimation && salaryman.animator != NULL)
    UpdateSalarymanAnimation(*salaryman.animator, delta_time);

  if (salaryman.useBezier) {
    const SalarymanPath &path = GetSalarymanPath();
    salaryman.bezierT +=
        (salaryman.speed / salaryman.pathTravelDistance) * delta_time;

    if (salaryman.bezierT >= 1.0f) {
      salaryman.active = false;
      return;
    }

    const float direction = salaryman.reverseBezier ? -1.0f : 1.0f;
    const float path_progress =
        salaryman.pathStartProgress +
        direction * salaryman.bezierT * salaryman.pathTravelDistance;
    const glm::vec3 new_pos = EvaluateSalarymanPath(path, path_progress);

    glm::vec3 dir = new_pos - salaryman.position;
    if (glm::length(dir) > 0.0001f) {
      salaryman.forward = glm::normalize(dir);
    }
    salaryman.position = new_pos;
  } else {
    salaryman.position += salaryman.forward * salaryman.speed * delta_time;
  }

  // Tick down the spawn grace timer
  if (salaryman.spawnGraceTimer > 0.0f)
    salaryman.spawnGraceTimer -= delta_time;

  // Despawn checks: for Bezier mode the bezierT >= 1.0 check above already
  // handles end-of-path despawn, so we only check player distance here.
  // For non-Bezier (linear) mode we also check corridor progress.
  const glm::vec3 camera_position(camera_position_c.x, camera_position_c.y,
                                  camera_position_c.z);
  const float player_distance =
      glm::length(salaryman.position - camera_position);

  if (!salaryman.useBezier) {
    // Linear mode: use the NPC's forward direction (not hardcoded Z) to measure
    // how far along the corridor the NPC has traveled.
    const glm::vec3 progress_dir = glm::normalize(salaryman.forward);
    const float corridor_progress =
        glm::dot(salaryman.position - salaryman.corridorOrigin, progress_dir);

    if (corridor_progress < -15.0f) {
      if (kCorridorDebugLogsEnabled) {
        printf("Salaryman despawn: reason=behind_spawn corridorId=%d "
               "progress=%.2f "
               "playerDistance=%.2f position=(%.2f, %.2f, %.2f), active=false\n",
               salaryman.corridorId, corridor_progress, player_distance,
               salaryman.position.x, salaryman.position.y,
               salaryman.position.z);
      }
      salaryman.active = false;
    } else if (corridor_progress > salaryman.corridorLength + 15.0f) {
      if (kCorridorDebugLogsEnabled) {
        printf("Salaryman despawn: reason=exited_corridor corridorId=%d "
               "progress=%.2f playerDistance=%.2f position=(%.2f, %.2f, %.2f), "
               "active=false\n",
               salaryman.corridorId, corridor_progress, player_distance,
               salaryman.position.x, salaryman.position.y,
               salaryman.position.z);
      }
      salaryman.active = false;
    }
  }

  // Only apply distance-based despawn after the grace period, because the
  // teleport coordinate wrapping can momentarily place the camera very far
  // from the NPC in canonical corridor space.
  if (salaryman.spawnGraceTimer <= 0.0f && player_distance > 55.0f) {
    salaryman.active = false;
  }
}

void DrawSalarymanNPC(const SalarymanNPC &salaryman, const Material &material) {
  if (!salaryman.active)
    return;

  glm::vec3 forward = salaryman.forward;
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
  const glm::vec3 p = salaryman.position;
  float scale = salaryman.isGiant ? 1.5f : 1.0f;
  const glm::mat4 model_matrix =
      Matrix(right.x * scale, corrected_up.x * scale, forward.x * scale, p.x, right.y * scale, corrected_up.y * scale,
             forward.y * scale, p.y, right.z * scale, corrected_up.z * scale, forward.z * scale, p.z, 0.0f,
             0.0f, 0.0f, 1.0f);

  ApplyMaterial(material);

  if (salaryman.useAnimation && salaryman.animatedModel != NULL &&
      salaryman.animatedModel->loaded) {
    const glm::mat4 animated_model_matrix =
        model_matrix * salaryman.animatedModel->normalizationMatrix;
    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                       glm::value_ptr(animated_model_matrix));
    DrawAnimatedModel(*salaryman.animatedModel);
  } else if (salaryman.model != NULL) {
    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                       glm::value_ptr(model_matrix));
    DrawStaticModel(*salaryman.model);
  }
}

void TrySpawnSalarymanForCorridorContent(const CorridorContent &content,
                                         const glm::vec3 &player_position,
                                         const char *reason) {
  const CorridorContentFrame &frame = content.frame;
  glm::vec3 spawn_forward = content.salarymanForward;
  if (glm::length(spawn_forward) < 0.0001f)
    spawn_forward = frame.contentForward;
  if (glm::length(spawn_forward) < 0.0001f)
    spawn_forward = glm::vec3(0.0f, 0.0f, -1.0f);
  else
    spawn_forward = glm::normalize(spawn_forward);

  const glm::vec3 spawn_position = content.salarymanSpawnPosition;
  const bool already_spawned =
      (g_LastSalarymanSpawnCorridorId == frame.logicalCorridorId);

  (void)reason;
  (void)player_position;
  (void)spawn_position;
  (void)spawn_forward;

  if (already_spawned)
    return;

  SpawnSalarymanForCorridor(g_SalarymanNPC, content, player_position);
  g_LastSalarymanSpawnCorridorId = frame.logicalCorridorId;
}
