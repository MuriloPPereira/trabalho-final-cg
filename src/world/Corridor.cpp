#include "world/Corridor.h"

#include "engine/Renderer.h"
#include "entities/NPC.h"
#include "matrices.h"
#include "rendering/Mesh.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

int g_CurrentExitLevel = 0;
bool g_GameWon = false;
int g_CurrentCorridorSequenceId = 0;
int g_NextCorridorSequenceId = 1;
int g_LastEnteredPhysicalSide = 0;
int g_LastSalarymanSpawnCorridorId = -1;
int g_PreparedNextCorridorId = -1;
int g_PreparedTransitionDirection = 0;
bool g_InConnectorTransition = false;
bool g_ConnectorMidpointCrossed = false;
int g_LastPlayerSection = -1;
CorridorInstance g_CurrentCorridorInstance;
CorridorInstance g_NegativeCandidateCorridorInstance;
CorridorInstance g_PositiveCandidateCorridorInstance;
CorridorAnomalyType g_ForceNextAnomalyType = kCorridorAnomalyCount;

int PositiveModulo(int value, int divisor) {
  int result = value % divisor;
  return (result < 0) ? result + divisor : result;
}

int GetPosterTextureIndex(int corridor_id, int poster_slot) {
  (void)corridor_id;
  return PositiveModulo(poster_slot, kPosterCount);
}

const char *CorridorAnomalyTypeName(CorridorAnomalyType anomaly_type) {
  switch (anomaly_type) {
  case kCorridorAnomalyNone:
    return "none";
  case kCorridorAnomalyIdenticalPosters:
    return "identical_posters";
  case kCorridorAnomalyNoSmokingSigns:
    return "no_smoking_signs";
  case kCorridorAnomalyCamouflagedPursuer:
    return "camouflaged_pursuer";
  case kCorridorAnomalyGiantNPC:
    return "giant_npc";
  case kCorridorAnomalyModifiedFloor:
    return "modified_floor";
  case kCorridorAnomalyTwoDoors:
    return "two_doors";
  case kCorridorAnomalyScaryPoster:
    return "scary_poster";
  case kCorridorAnomalyDoorKnocking:
    return "door_knocking";
  default:
    return "unknown";
  }
}

static CorridorAnomalyType ChooseRandomAnomalyType() {
  const int anomaly_count =
      static_cast<int>(kCorridorAnomalyCount) -
      static_cast<int>(kCorridorAnomalyIdenticalPosters);
  return static_cast<CorridorAnomalyType>(
      static_cast<int>(kCorridorAnomalyIdenticalPosters) +
      PositiveModulo(rand(), anomaly_count));
}

static unsigned int HashCorridorValue(int corridor_id, int index, int salt) {
  unsigned int x = 2166136261u;
  x ^= static_cast<unsigned int>(corridor_id + 1009);
  x *= 16777619u;
  x ^= static_cast<unsigned int>(index + 9176);
  x *= 16777619u;
  x ^= static_cast<unsigned int>(salt + 131);
  x *= 16777619u;
  x ^= x >> 13;
  x *= 1274126177u;
  x ^= x >> 16;
  return x;
}

static float HashToUnitFloat(int corridor_id, int index, int salt) {
  return static_cast<float>(HashCorridorValue(corridor_id, index, salt) &
                            0x00ffffffu) /
         static_cast<float>(0x01000000u);
}

static float RandomRange(int corridor_id, int index, int salt, float min_value,
                         float max_value) {
  return min_value +
         (max_value - min_value) * HashToUnitFloat(corridor_id, index, salt);
}

static bool SignDistanceOverlapsDoorway(float distance, float sign_width) {
  const float sign_half_extent = sign_width * 0.5f + 0.18f;
  const float doorway_half_extent = kDoorwayOpeningWidth * 0.5f;
  for (int slot = 0; slot < kDoorwayCount; ++slot) {
    const float doorway_distance =
        kCorridorLength * kDoorwayDistanceFractions[slot];
    if (std::abs(distance - doorway_distance) <
        sign_half_extent + doorway_half_extent)
      return true;
  }
  return false;
}

static NoSmokingSignInstance MakeNoSmokingSign(
    const CorridorContentFrame &frame, float distance, float center_y,
    float width, float height, float rotation_radians, float surface_offset) {
  const glm::vec3 wall_normal = -frame.contentRight;
  const glm::vec3 base_up(0.0f, 1.0f, 0.0f);
  const glm::vec3 base_width_axis = frame.contentForward;
  const float c = std::cos(rotation_radians);
  const float s = std::sin(rotation_radians);

  NoSmokingSignInstance sign;
  sign.normal = wall_normal;
  sign.up = glm::normalize(base_up * c + base_width_axis * s);
  sign.widthAxis = glm::normalize(base_width_axis * c - base_up * s);
  sign.width = width;
  sign.height = height;
  sign.position = frame.contentOrigin +
                  frame.contentRight * kCorridorHalfWidth +
                  wall_normal * surface_offset +
                  frame.contentForward * distance;
  sign.position.y = center_y;
  return sign;
}

static void AddNormalNoSmokingSign(std::vector<NoSmokingSignInstance> &signs,
                                   const CorridorContentFrame &frame) {
  signs.push_back(MakeNoSmokingSign(
      frame, kNoSmokingSignStartDistance, kNoSmokingSignCenterY,
      kNoSmokingSignWidth, kNoSmokingSignHeight, 0.0f,
      kNoSmokingSignWallOffset));
}

static void AddNoSmokingAnomalySigns(std::vector<NoSmokingSignInstance> &signs,
                                     const CorridorContentFrame &frame,
                                     int corridor_id) {
  signs.reserve(kNoSmokingAnomalySignCount);

  float previous_distance = kNoSmokingSignStartDistance;
  float previous_y = kNoSmokingSignCenterY;
  for (int i = 0; i < kNoSmokingAnomalySignCount; ++i) {
    const float scale = RandomRange(corridor_id, i, 10, 0.65f, 1.35f);
    const float width = kNoSmokingSignWidth * scale;
    const float height = kNoSmokingSignHeight * scale;
    const float distance_min = 0.85f + width * 0.5f;
    const float distance_max = frame.corridorLength - 0.85f - width * 0.5f;
    const float y_min = 0.35f + height * 0.5f;
    const float y_max = kCorridorHeight - 0.22f - height * 0.5f;

    float distance =
        RandomRange(corridor_id, i, 20, distance_min, distance_max);
    float center_y = RandomRange(corridor_id, i, 30, y_min, y_max);

    if (i > 0 && i % 7 == 0) {
      distance = previous_distance +
                 RandomRange(corridor_id, i, 40, -0.35f, 0.35f);
      center_y = previous_y + RandomRange(corridor_id, i, 50, -0.26f, 0.26f);
      distance = std::max(distance_min, std::min(distance, distance_max));
      center_y = std::max(y_min, std::min(center_y, y_max));
    }

    for (int attempt = 0;
         attempt < 6 && SignDistanceOverlapsDoorway(distance, width);
         ++attempt) {
      distance = RandomRange(corridor_id, i, 100 + attempt, distance_min,
                             distance_max);
    }

    const float rotation =
        RandomRange(corridor_id, i, 60, -0.22f, 0.22f);
    const float surface_offset =
        kNoSmokingSignWallOffset + i * kNoSmokingSignLayerOffset;
    signs.push_back(MakeNoSmokingSign(frame, distance, center_y, width, height,
                                      rotation, surface_offset));
    previous_distance = distance;
    previous_y = center_y;
  }
}

CorridorContent GenerateCorridorContent(int corridor_id,
                                        const glm::vec3 &content_forward,
                                        CorridorAnomalyType anomaly_type);
CorridorInstance CreateNewCorridorInstance(int logical_id,
                                           const glm::vec3 &content_forward,
                                           CorridorAnomalyType anomaly_type);

CorridorState MakeCorridorState(int id) {
  CorridorState state;
  state.id = id;
  state.has_anomaly = false;
  state.anomaly_type = kCorridorAnomalyNone;
  state.is_tutorial = false;
  state.entrance_progress = g_CurrentExitLevel;
  return state;
}

static CorridorState MakeCorridorState(int id,
                                       CorridorAnomalyType anomaly_type) {
  CorridorState state = MakeCorridorState(id);
  state.has_anomaly = (anomaly_type != kCorridorAnomalyNone);
  state.anomaly_type = anomaly_type;
  return state;
}

static CorridorInstance CreateCorridorInstanceFromState(
    const CorridorState &state, const glm::vec3 &content_forward) {
  CorridorInstance instance;
  instance.state = state;
  instance.content =
      GenerateCorridorContent(state.id, content_forward, state.anomaly_type);
  return instance;
}

void RefreshCandidateCorridorStates() {

  bool next_has_anomaly = (rand() % 2 == 0);
  CorridorAnomalyType next_anomaly_type =
      next_has_anomaly ? ChooseRandomAnomalyType() : kCorridorAnomalyNone;

  if (g_ForceNextAnomalyType != kCorridorAnomalyCount) {
    next_anomaly_type = g_ForceNextAnomalyType;
    next_has_anomaly = (next_anomaly_type != kCorridorAnomalyNone);
    g_ForceNextAnomalyType = kCorridorAnomalyCount;
  }

  const CorridorState next_state =
      MakeCorridorState(g_NextCorridorSequenceId, next_anomaly_type);

  g_NegativeCandidateCorridorInstance = CreateCorridorInstanceFromState(
      next_state, glm::vec3(0.0f, 0.0f, +1.0f));
  g_PositiveCandidateCorridorInstance = CreateCorridorInstanceFromState(
      next_state, glm::vec3(0.0f, 0.0f, -1.0f));
}

void ForceNextCorridorAnomaly(CorridorAnomalyType anomaly_type) {
  g_ForceNextAnomalyType = anomaly_type;
  RefreshCandidateCorridorStates();
}

void InitializeCorridorLifecycle() {
  g_CurrentExitLevel = 0;
  g_GameWon = false;
  g_CurrentCorridorSequenceId = 0;
  g_NextCorridorSequenceId = 1;
  g_LastEnteredPhysicalSide = 0;
  g_LastSalarymanSpawnCorridorId = -1;
  g_PreparedNextCorridorId = -1;
  g_PreparedTransitionDirection = 0;
  g_InConnectorTransition = false;
  g_ConnectorMidpointCrossed = false;
  g_LastPlayerSection = -1;
  CorridorState tutorial_state = MakeCorridorState(g_CurrentCorridorSequenceId);
  tutorial_state.is_tutorial = true;
  tutorial_state.entrance_progress = -1;
  g_CurrentCorridorInstance = CreateCorridorInstanceFromState(
      tutorial_state, glm::vec3(0.0f, 0.0f, -1.0f));
  RefreshCandidateCorridorStates();
}

static bool IsCorridorDecisionCorrect(int physical_side) {
  const float player_moved_z = -static_cast<float>(physical_side);
  const bool went_forward =
      (player_moved_z ==
       g_CurrentCorridorInstance.content.frame.contentForward.z);
  const bool had_anomaly = g_CurrentCorridorInstance.state.has_anomaly;
  return (went_forward && !had_anomaly) || (!went_forward && had_anomaly);
}

void PrepareCorridorTransition(int physical_side) {
  if (physical_side == 0 || g_GameWon)
    return;

  CorridorInstance &next_corridor =
      (physical_side < 0) ? g_NegativeCandidateCorridorInstance
                          : g_PositiveCandidateCorridorInstance;

  if (g_CurrentCorridorInstance.state.is_tutorial) {
    next_corridor.state.is_tutorial = false;
    next_corridor.state.entrance_progress = g_CurrentExitLevel;
  } else if (IsCorridorDecisionCorrect(physical_side)) {
    next_corridor.state.is_tutorial = false;
    next_corridor.state.entrance_progress =
        std::min(g_CurrentExitLevel + 1, 8);
  } else {
    // A wrong choice enters the same unnumbered reference corridor used by
    // InitializeCorridorLifecycle(). Hide the candidate sign immediately;
    // the full reset still happens when the transition is committed.
    next_corridor.state.is_tutorial = true;
    next_corridor.state.entrance_progress = -1;
  }
}

void ActivateNewLogicalCorridor(int physical_side) {
  if (physical_side == 0 || g_GameWon)
    return;


  if (g_CurrentCorridorInstance.state.is_tutorial) {
    printf("\n--- REFERENCE CORRIDOR COMPLETE -> EXIT LEVEL: 0 ---\n\n");
  } else {
    // Continuing forward is correct only in a normal corridor; turning back
    // is correct only when the current corridor contains an anomaly.
    const bool is_correct = IsCorridorDecisionCorrect(physical_side);
    if (is_correct) {
      g_CurrentExitLevel = std::min(g_CurrentExitLevel + 1, 8);
      if (g_CurrentExitLevel == 8) {
        g_GameWon = true;
        printf("\n*** YOU WIN! You crossed Exit 8! ***\n\n");
      } else {
        printf("\n--- CORRECT -> EXIT LEVEL: %d ---\n\n",
               g_CurrentExitLevel);
      }
    } else {
      // A wrong decision restarts the same unnumbered, anomaly-free reference
      // corridor lifecycle used after a pursuer catch. The transition system
      // will still recenter the player and activate entities for this freshly
      // initialized tutorial corridor after this function returns.
      InitializeCorridorLifecycle();
      printf("\n--- INCORRECT -> REFERENCE CORRIDOR (EXIT LEVEL: %d) ---\n\n",
             g_CurrentExitLevel);
      fflush(stdout);
      return;
    }
  }

  fflush(stdout);

  g_LastEnteredPhysicalSide = physical_side;
  g_CurrentCorridorInstance = (physical_side < 0)
                                  ? g_NegativeCandidateCorridorInstance
                                  : g_PositiveCandidateCorridorInstance;
  if (g_CurrentExitLevel == 8) {
    const int exit_corridor_id = g_CurrentCorridorInstance.state.id;
    const glm::vec3 exit_corridor_forward =
        g_CurrentCorridorInstance.content.frame.contentForward;
    g_CurrentCorridorInstance = CreateNewCorridorInstance(
        exit_corridor_id, exit_corridor_forward, kCorridorAnomalyNone);
  }
  g_CurrentCorridorInstance.state.is_tutorial = false;
  g_CurrentCorridorInstance.state.entrance_progress = g_CurrentExitLevel;

  if (kCorridorDebugLogsEnabled) {
    if (g_CurrentCorridorInstance.state.has_anomaly) {
      printf("\n[SPOILER] ANOMALIA NO CORREDOR ATUAL (%s)\n",
             CorridorAnomalyTypeName(
                 g_CurrentCorridorInstance.state.anomaly_type));
    } else {
      printf("\n[SPOILER] CORREDOR NORMAL\n");
    }
    fflush(stdout);
  }
  g_CurrentCorridorSequenceId = g_CurrentCorridorInstance.state.id;
  g_NextCorridorSequenceId = g_CurrentCorridorSequenceId + 1;
  RefreshCandidateCorridorStates();
}

CanonicalCorridorLayout GetCanonicalCorridorLayout() {
  CanonicalCorridorLayout layout;
  layout.connector_length = kConnectorLength;
  layout.turn_z0 = kCorridorZ1;
  layout.turn_z1 = layout.turn_z0 - kCornerLength;
  layout.connector_center_z = layout.turn_z0 - 0.5f * kCornerLength;
  layout.connector_start_x = -kCorridorHalfWidth;
  layout.connector_end_x = layout.connector_start_x - layout.connector_length;
  layout.exit_turn_x = layout.connector_end_x - kCorridorHalfWidth;
  layout.corridor2_offset_x = layout.exit_turn_x;
  layout.second_corridor_z_offset = layout.turn_z1;
  layout.block_offset =
      glm::vec2(layout.corridor2_offset_x, layout.second_corridor_z_offset);
  return layout;
}

glm::vec3 TransformPoint(const glm::mat4 &transform, const glm::vec3 &point) {
  glm::vec4 p = transform * glm::vec4(point.x, point.y, point.z, 1.0f);
  return glm::vec3(p.x, p.y, p.z);
}

glm::vec3 TransformVector(const glm::mat4 &transform, const glm::vec3 &vector) {
  glm::vec4 v = transform * glm::vec4(vector.x, vector.y, vector.z, 0.0f);
  return glm::vec3(v.x, v.y, v.z);
}

CorridorRenderTransform
MakeCorridorRenderTransform(const glm::mat4 &geometry_from_local) {
  CorridorRenderTransform transform;
  transform.geometryFromLocal = geometry_from_local;
  return transform;
}

CorridorContentFrame
MakeCorridorContentFrame(int logical_corridor_id,
                         const glm::vec3 &requested_content_forward) {
  const CanonicalCorridorLayout layout = GetCanonicalCorridorLayout();
  const glm::vec3 world_up(0.0f, 1.0f, 0.0f);

  CorridorContentFrame frame;
  frame.logicalCorridorId = logical_corridor_id;
  frame.contentForward = requested_content_forward;
  if (glm::length(frame.contentForward) < 0.0001f)
    frame.contentForward = glm::vec3(0.0f, 0.0f, -1.0f);
  else
    frame.contentForward = glm::normalize(frame.contentForward);

  frame.contentRight = glm::cross(frame.contentForward, world_up);
  if (glm::length(frame.contentRight) < 0.0001f)
    frame.contentRight = glm::vec3(1.0f, 0.0f, 0.0f);
  else
    frame.contentRight = glm::normalize(frame.contentRight);

  frame.contentOrigin =
      (glm::dot(frame.contentForward, glm::vec3(0.0f, 0.0f, -1.0f)) >= 0.0f)
          ? glm::vec3(0.0f, 0.0f, kCorridorZ0)
          : glm::vec3(0.0f, 0.0f, kCorridorZ1);
  frame.corridorLength = kCorridorLength;
  frame.connectorLength = layout.connector_length;
  frame.posterWallSide = "canonical_left";
  return frame;
}

CorridorContent GenerateCorridorContent(int corridor_id,
                                        const glm::vec3 &content_forward,
                                        CorridorAnomalyType anomaly_type) {
  const float poster_center_y = 1.6f;
  const float poster_offset = 0.02f;
  const float poster_wall_offset = kCorridorHalfWidth - poster_offset;
  const float spacing = kCorridorLength / (kPosterCount + 1);

  CorridorContent content;
  content.corridorId = corridor_id;
  content.frame = MakeCorridorContentFrame(corridor_id, content_forward);
  content.posters.reserve(kPosterCount);
  const int doorway_count =
      (anomaly_type == kCorridorAnomalyTwoDoors) ? kDoorwayCount - 1
                                                 : kDoorwayCount;
  content.doorways.reserve(doorway_count);
  content.lightPositions.reserve(7);
  content.hasAnomaly = (anomaly_type != kCorridorAnomalyNone);
  content.anomalyType = anomaly_type;

  const CorridorContentFrame &frame = content.frame;
  content.camouflagedPursuerSpawnPosition =
      frame.contentOrigin +
      frame.contentForward *
          (frame.corridorLength + kCornerLength -
           kCamouflagedPursuerEndWallClearance);
  content.camouflagedPursuerSpawnPosition.y = 0.0f;
  for (int slot = 0; slot < kPosterCount; ++slot) {
    const float poster_distance = (slot + 1) * spacing;

    PosterSlotLayout poster;
    poster.slot = slot;
    if (anomaly_type == kCorridorAnomalyIdenticalPosters) {
      poster.textureIndex = 0;
    } else if (anomaly_type == kCorridorAnomalyScaryPoster &&
               slot == kScaryPosterSlot) {
      poster.textureIndex = kScaryPosterTextureIndex;
    } else {
      poster.textureIndex = GetPosterTextureIndex(corridor_id, slot);
    }
    poster.position = frame.contentOrigin -
                      frame.contentRight * poster_wall_offset +
                      frame.contentForward * poster_distance;
    poster.position.y = poster_center_y;
    poster.normal = frame.contentRight;
    poster.up = glm::vec3(0.0f, 1.0f, 0.0f);
    poster.widthAxis = -frame.contentForward;
    poster.wallSide = frame.posterWallSide;
    content.posters.push_back(poster);
  }

  if (anomaly_type == kCorridorAnomalyNoSmokingSigns)
    AddNoSmokingAnomalySigns(content.noSmokingSigns, frame, corridor_id);
  else
    AddNormalNoSmokingSign(content.noSmokingSigns, frame);

  for (int slot = 0; slot < doorway_count; ++slot) {
    const float doorway_distance =
        frame.corridorLength * kDoorwayDistanceFractions[slot];

    DoorInstance doorway;
    doorway.slot = slot;
    doorway.width = kDoorwayOpeningWidth;
    doorway.height = kDoorwayOpeningHeight;
    doorway.recessDepth = kDoorwayRecessDepth;
    doorway.position =
        frame.contentOrigin + frame.contentForward * doorway_distance +
        frame.contentRight * (kCorridorHalfWidth + kDoorwayRecessDepth -
                              kDoorwayPanelInset);
    doorway.position.y = 0.0f;
    doorway.normal = -frame.contentRight;
    doorway.up = glm::vec3(0.0f, 1.0f, 0.0f);
    doorway.widthAxis = frame.contentForward;
    doorway.attachmentName = "future_door_model";
    content.doorways.push_back(doorway);
  }

  const CanonicalCorridorLayout layout = GetCanonicalCorridorLayout();
  const float light_y = kCorridorHeight - 0.15f;
  const float straight_light_spacing = kCorridorLength / 5.0f;
  for (int i = 0; i < 4; ++i) {
    glm::vec3 light_position =
        frame.contentOrigin +
        frame.contentForward * ((i + 1) * straight_light_spacing);
    light_position.y = light_y;
    content.lightPositions.push_back(light_position);
  }
  content.lightPositions.push_back(
      glm::vec3(0.0f, light_y, layout.turn_z0 - 0.5f * kCornerLength));
  content.lightPositions.push_back(
      glm::vec3(layout.connector_start_x - 0.5f * layout.connector_length,
                light_y, layout.connector_center_z));
  content.lightPositions.push_back(glm::vec3(
      layout.exit_turn_x, light_y, layout.turn_z0 - 0.5f * kCornerLength));

  content.salarymanForward = -frame.contentForward;
  content.salarymanSpawnPosition = ComputeSalarymanSpawnPosition(frame);

  return content;
}

CorridorInstance CreateNewCorridorInstance(int logical_id,
                                           const glm::vec3 &content_forward,
                                           CorridorAnomalyType anomaly_type) {
  return CreateCorridorInstanceFromState(
      MakeCorridorState(logical_id, anomaly_type), content_forward);
}

std::string PosterOrderString(const CorridorContent &content) {
  std::string poster_order;
  for (const PosterSlotLayout &poster : content.posters) {
    if (!poster_order.empty())
      poster_order += ",";
    poster_order += std::to_string(poster.textureIndex);
  }
  return poster_order;
}

void LogCorridorContentSignature(const char *reason, int render_slot,
                                 int traversal_direction,
                                 const CorridorInstance &corridor_instance,
                                 const glm::vec3 &block_display_offset) {
  if (!kCorridorDebugLogsEnabled)
    return;

  const CorridorContent &content = corridor_instance.content;
  const CorridorContentFrame &frame = content.frame;
  const std::string poster_order = PosterOrderString(content);
  const char *poster_wall_side =
      content.posters.empty() ? "none" : content.posters[0].wallSide;

  printf("Corridor window content: reason=%s, renderSlot=%d, "
         "traversalDirection=%d, currentCorridorId=%d, corridorId=%d, "
         "blockOffset=(%.2f, %.2f, %.2f), contentOrigin=(%.2f, %.2f, %.2f), "
         "contentForward=(%.2f, %.2f, %.2f), contentRight=(%.2f, %.2f, %.2f), "
         "posterOrder=[%s], posterWallSide=%s, npcSpawnPosition=(%.2f, %.2f, "
         "%.2f), npcForward=(%.2f, %.2f, %.2f), lightCount=%d, anomaly=%s\n",
         reason, render_slot, traversal_direction,
         g_CurrentCorridorInstance.state.id, content.corridorId,
         block_display_offset.x, block_display_offset.y, block_display_offset.z,
         frame.contentOrigin.x, frame.contentOrigin.y, frame.contentOrigin.z,
         frame.contentForward.x, frame.contentForward.y, frame.contentForward.z,
         frame.contentRight.x, frame.contentRight.y, frame.contentRight.z,
         poster_order.c_str(), poster_wall_side,
         content.salarymanSpawnPosition.x, content.salarymanSpawnPosition.y,
         content.salarymanSpawnPosition.z, content.salarymanForward.x,
         content.salarymanForward.y, content.salarymanForward.z,
         static_cast<int>(content.lightPositions.size()),
         content.hasAnomaly ? "true" : "false");

  for (const PosterSlotLayout &poster : content.posters) {
    const glm::mat4 poster_basis = Matrix(
        poster.normal.x, poster.up.x, poster.widthAxis.x, poster.position.x,
        poster.normal.y, poster.up.y, poster.widthAxis.y, poster.position.y,
        poster.normal.z, poster.up.z, poster.widthAxis.z, poster.position.z,
        0.0f, 0.0f, 0.0f, 1.0f);

    printf("Corridor window poster: reason=%s, renderSlot=%d, "
           "traversalDirection=%d, corridorId=%d, posterSlot=%d, "
           "posterTextureIndex=%d, posterWallSide=%s, "
           "posterCanonicalPosition=(%.2f, %.2f, %.2f), posterNormal=(%.2f, "
           "%.2f, %.2f), posterTransform=[%.2f %.2f %.2f %.2f | %.2f %.2f %.2f "
           "%.2f | %.2f %.2f %.2f %.2f | %.2f %.2f %.2f %.2f]\n",
           reason, render_slot, traversal_direction, content.corridorId,
           poster.slot, poster.textureIndex, poster.wallSide, poster.position.x,
           poster.position.y, poster.position.z, poster.normal.x,
           poster.normal.y, poster.normal.z, poster_basis[0][0],
           poster_basis[1][0], poster_basis[2][0], poster_basis[3][0],
           poster_basis[0][1], poster_basis[1][1], poster_basis[2][1],
           poster_basis[3][1], poster_basis[0][2], poster_basis[1][2],
           poster_basis[2][2], poster_basis[3][2], poster_basis[0][3],
           poster_basis[1][3], poster_basis[2][3], poster_basis[3][3]);
  }
}

void LogCorridorSlotStability(const char *reason, int traversal_direction,
                              const CorridorInstance &corridor_instance) {
  if (!kCorridorDebugLogsEnabled)
    return;

  const CanonicalCorridorLayout layout = GetCanonicalCorridorLayout();
  const glm::vec2 block_offset = layout.block_offset;
  const glm::vec3 slot_offsets[] = {
      glm::vec3(-block_offset.x, 0.0f, -block_offset.y),
      glm::vec3(0.0f, 0.0f, 0.0f),
      glm::vec3(block_offset.x, 0.0f, block_offset.y)};
  const int render_slots[] = {-1, 0, +1};

  const CorridorContent &content = corridor_instance.content;
  const CorridorContentFrame &frame = content.frame;
  const std::string poster_order = PosterOrderString(content);
  const char *poster_wall_side =
      content.posters.empty() ? "none" : content.posters[0].wallSide;

  for (int i = 0; i < 3; ++i) {
    printf("Corridor slot stability: reason=%s, traversalDirection=%d, "
           "corridorId=%d, renderSlot=%d, blockOffset=(%.2f, %.2f, %.2f), "
           "contentOrigin=(%.2f, %.2f, %.2f), contentForward=(%.2f, %.2f, "
           "%.2f), contentRight=(%.2f, %.2f, %.2f), posterOrder=[%s], "
           "posterWallSide=%s, npcSpawnPosition=(%.2f, %.2f, %.2f), "
           "npcForward=(%.2f, %.2f, %.2f), signatureStable=true\n",
           reason, traversal_direction, content.corridorId, render_slots[i],
           slot_offsets[i].x, slot_offsets[i].y, slot_offsets[i].z,
           frame.contentOrigin.x, frame.contentOrigin.y, frame.contentOrigin.z,
           frame.contentForward.x, frame.contentForward.y,
           frame.contentForward.z, frame.contentRight.x, frame.contentRight.y,
           frame.contentRight.z, poster_order.c_str(), poster_wall_side,
           content.salarymanSpawnPosition.x, content.salarymanSpawnPosition.y,
           content.salarymanSpawnPosition.z, content.salarymanForward.x,
           content.salarymanForward.y, content.salarymanForward.z);
  }
}

void LogCorridorWindow(const char *reason, int traversal_direction) {
  if (!kCorridorDebugLogsEnabled)
    return;

  const CanonicalCorridorLayout layout = GetCanonicalCorridorLayout();
  const glm::vec2 block_offset = layout.block_offset;

  printf("Corridor window update: reason=%s, traversalDirection=%d, "
         "currentCorridorId=%d, activeCorridorIds=[%d,%d,%d], "
         "lastEnteredPhysicalSide=%d, preparedCorridorId=%d, "
         "lastSpawnedCorridorId=%d, salarymanActive=%s\n",
         reason, traversal_direction, g_CurrentCorridorInstance.state.id,
         g_NegativeCandidateCorridorInstance.state.id,
         g_CurrentCorridorInstance.state.id,
         g_PositiveCandidateCorridorInstance.state.id,
         g_LastEnteredPhysicalSide, g_PreparedNextCorridorId,
         g_LastSalarymanSpawnCorridorId,
         g_SalarymanNPC.active ? "true" : "false");

  LogCorridorContentSignature(
      reason, -1, traversal_direction, g_NegativeCandidateCorridorInstance,
      glm::vec3(-block_offset.x, 0.0f, -block_offset.y));
  LogCorridorContentSignature(reason, 0, traversal_direction,
                              g_CurrentCorridorInstance,
                              glm::vec3(0.0f, 0.0f, 0.0f));
  LogCorridorContentSignature(reason, +1, traversal_direction,
                              g_PositiveCandidateCorridorInstance,
                              glm::vec3(block_offset.x, 0.0f, block_offset.y));
  LogCorridorSlotStability(reason, traversal_direction,
                           g_CurrentCorridorInstance);
}

void LogCorridorTransition(const char *reason, int traversal_direction,
                           const CorridorContent &content,
                           const glm::vec3 &player_position) {
  if (!kCorridorDebugLogsEnabled)
    return;

  const CorridorContentFrame &frame = content.frame;
  const std::string poster_order = PosterOrderString(content);
  const char *poster_wall_side =
      content.posters.empty() ? "none" : content.posters[0].wallSide;

  printf("Corridor transition: reason=%s, traversalDirection=%d, "
         "corridorId=%d, contentForward=(%.2f, %.2f, %.2f), "
         "contentRight=(%.2f, %.2f, %.2f), posterOrder=[%s], "
         "posterWallSide=%s, npcSpawnPosition=(%.2f, %.2f, %.2f), "
         "npcForward=(%.2f, %.2f, %.2f), playerPosition=(%.2f, %.2f, %.2f)\n",
         reason, traversal_direction, content.corridorId,
         frame.contentForward.x, frame.contentForward.y, frame.contentForward.z,
         frame.contentRight.x, frame.contentRight.y, frame.contentRight.z,
         poster_order.c_str(), poster_wall_side,
         content.salarymanSpawnPosition.x, content.salarymanSpawnPosition.y,
         content.salarymanSpawnPosition.z, content.salarymanForward.x,
         content.salarymanForward.y, content.salarymanForward.z,
         player_position.x, player_position.y, player_position.z);
}

glm::vec3 ComputeSalarymanSpawnPosition(const CorridorContentFrame &frame) {
  const CanonicalCorridorLayout layout = GetCanonicalCorridorLayout();
  const float connector_inset =
      std::min(kCornerLength, 0.5f * layout.connector_length);
  const glm::vec3 salaryman_forward = -frame.contentForward;
  const bool moves_canonical_forward =
      glm::dot(salaryman_forward, glm::vec3(0.0f, 0.0f, -1.0f)) >= 0.0f;

  if (moves_canonical_forward) {
    // Enter through the previous block's connector, whose exit turn joins the
    // visible corridor at the canonical origin.
    return glm::vec3(layout.connector_end_x - layout.block_offset.x +
                         connector_inset,
                     0.0f,
                     layout.connector_center_z - layout.block_offset.y);
  }

  // Enter in reverse through the current block's connector and its first turn.
  return glm::vec3(layout.connector_start_x - connector_inset, 0.0f,
                   layout.connector_center_z);
}

const char *PlayerSectionName(int player_section) {
  return (player_section == 1) ? "connector/turn" : "straight";
}

void BuildPostersAndAddToVirtualScene() {
  struct PosterVertex {
    float px, py, pz, pw;
    float nx, ny, nz, nw;
    float u, v;
  };

  std::vector<PosterVertex> vertices;
  std::vector<GLuint> indices;

  GLuint vertex_array_object_id;
  glGenVertexArrays(1, &vertex_array_object_id);
  glBindVertexArray(vertex_array_object_id);

  auto add_poster_quad = [&](const std::string &name, glm::vec3 p0,
                             glm::vec3 p1, glm::vec3 p2, glm::vec3 p3,
                             glm::vec3 normal) {
    size_t first_index = indices.size();
    GLuint base_vertex = static_cast<GLuint>(vertices.size());

    auto push_vertex = [&](glm::vec3 p, float u, float v) {
      PosterVertex vertex;
      vertex.px = p.x;
      vertex.py = p.y;
      vertex.pz = p.z;
      vertex.pw = 1.0f;
      vertex.nx = normal.x;
      vertex.ny = normal.y;
      vertex.nz = normal.z;
      vertex.nw = 0.0f;
      vertex.u = u;
      vertex.v = v;
      vertices.push_back(vertex);
    };

    // Rotate the poster image 90 degrees counterclockwise on its own axis.
    push_vertex(p0, 1.0f, 0.0f);
    push_vertex(p1, 1.0f, 1.0f);
    push_vertex(p2, 0.0f, 1.0f);
    push_vertex(p3, 0.0f, 0.0f);

    indices.push_back(base_vertex + 0);
    indices.push_back(base_vertex + 1);
    indices.push_back(base_vertex + 2);
    indices.push_back(base_vertex + 0);
    indices.push_back(base_vertex + 2);
    indices.push_back(base_vertex + 3);

    glm::vec3 bbox_min = p0;
    glm::vec3 bbox_max = p0;
    bbox_min.x = std::min(bbox_min.x, p1.x);
    bbox_min.y = std::min(bbox_min.y, p1.y);
    bbox_min.z = std::min(bbox_min.z, p1.z);
    bbox_max.x = std::max(bbox_max.x, p1.x);
    bbox_max.y = std::max(bbox_max.y, p1.y);
    bbox_max.z = std::max(bbox_max.z, p1.z);
    bbox_min.x = std::min(bbox_min.x, p2.x);
    bbox_min.y = std::min(bbox_min.y, p2.y);
    bbox_min.z = std::min(bbox_min.z, p2.z);
    bbox_max.x = std::max(bbox_max.x, p2.x);
    bbox_max.y = std::max(bbox_max.y, p2.y);
    bbox_max.z = std::max(bbox_max.z, p2.z);
    bbox_min.x = std::min(bbox_min.x, p3.x);
    bbox_min.y = std::min(bbox_min.y, p3.y);
    bbox_min.z = std::min(bbox_min.z, p3.z);
    bbox_max.x = std::max(bbox_max.x, p3.x);
    bbox_max.y = std::max(bbox_max.y, p3.y);
    bbox_max.z = std::max(bbox_max.z, p3.z);

    SceneObject object;
    object.name = name;
    object.first_index = first_index;
    object.num_indices = 6;
    object.rendering_mode = GL_TRIANGLES;
    object.vertex_array_object_id = vertex_array_object_id;
    object.bbox_min = bbox_min;
    object.bbox_max = bbox_max;
    g_VirtualScene[name] = object;
  };

  const float poster_width = 1.8f;
  const float poster_height = 1.5f;

  for (int i = 0; i < kPosterCount; ++i) {
    float y0 = -poster_height * 0.5f;
    float y1 = +poster_height * 0.5f;
    float z0 = -poster_width * 0.5f;
    float z1 = +poster_width * 0.5f;

    std::string name = "poster_" + std::to_string(i);
    add_poster_quad(name, glm::vec3(0.0f, y0, z0), glm::vec3(0.0f, y1, z0),
                    glm::vec3(0.0f, y1, z1), glm::vec3(0.0f, y0, z1),
                    glm::vec3(1.0f, 0.0f, 0.0f));
  }

  GLuint vertex_buffer_id;
  glGenBuffers(1, &vertex_buffer_id);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(PosterVertex),
               vertices.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(PosterVertex),
                        (void *)offsetof(PosterVertex, px));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(PosterVertex),
                        (void *)offsetof(PosterVertex, nx));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(PosterVertex),
                        (void *)offsetof(PosterVertex, u));
  glEnableVertexAttribArray(2);

  GLuint index_buffer_id;
  glGenBuffers(1, &index_buffer_id);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_id);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint),
               indices.data(), GL_STATIC_DRAW);

  glBindVertexArray(0);
}

void BuildNoSmokingSignAndAddToVirtualScene() {
  struct SignVertex {
    float px, py, pz, pw;
    float nx, ny, nz, nw;
    float u, v;
  };

  const SignVertex vertices[] = {
      {0.0f, -0.5f, -0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, +0.5f, -0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
      {0.0f, +0.5f, +0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f},
      {0.0f, -0.5f, +0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f},
  };
  const GLuint indices[] = {0, 1, 2, 0, 2, 3};

  GLuint vertex_array_object_id;
  glGenVertexArrays(1, &vertex_array_object_id);
  glBindVertexArray(vertex_array_object_id);

  GLuint vertex_buffer_id;
  glGenBuffers(1, &vertex_buffer_id);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(SignVertex),
                        (void *)offsetof(SignVertex, px));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(SignVertex),
                        (void *)offsetof(SignVertex, nx));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SignVertex),
                        (void *)offsetof(SignVertex, u));
  glEnableVertexAttribArray(2);

  GLuint index_buffer_id;
  glGenBuffers(1, &index_buffer_id);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_id);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
               GL_STATIC_DRAW);

  SceneObject object;
  object.name = "no_smoking_sign";
  object.first_index = 0;
  object.num_indices = 6;
  object.rendering_mode = GL_TRIANGLES;
  object.vertex_array_object_id = vertex_array_object_id;
  object.bbox_min = glm::vec3(0.0f, -0.5f, -0.5f);
  object.bbox_max = glm::vec3(0.0f, +0.5f, +0.5f);
  g_VirtualScene[object.name] = object;

  glBindVertexArray(0);
}

void BuildCorridorAndAddToVirtualScene() {
  struct CorridorVertex {
    float px, py, pz, pw;
    float nx, ny, nz, nw;
    float u, v;
  };

  std::vector<CorridorVertex> vertices;
  std::vector<GLuint> indices;

  GLuint vertex_array_object_id;
  glGenVertexArrays(1, &vertex_array_object_id);
  glBindVertexArray(vertex_array_object_id);

  auto add_quad = [&](const std::string &name, glm::vec3 p0, glm::vec3 p1,
                      glm::vec3 p2, glm::vec3 p3, glm::vec3 normal,
                      glm::vec3 uv_origin, glm::vec3 uv_u_axis,
                      glm::vec3 uv_v_axis, float u_tile_size,
                      float v_tile_size) {
    size_t first_index = indices.size();
    GLuint base_vertex = static_cast<GLuint>(vertices.size());

    auto make_uv = [&](const glm::vec3 &p) {
      glm::vec3 u_axis = glm::normalize(uv_u_axis);
      glm::vec3 v_axis = glm::normalize(uv_v_axis);
      glm::vec3 delta = p - uv_origin;
      float u = glm::dot(delta, u_axis);
      float v = glm::dot(delta, v_axis);
      return glm::vec2(u / u_tile_size, v / v_tile_size);
    };

    auto push_vertex = [&](glm::vec3 p, float u, float v) {
      CorridorVertex vertex;
      vertex.px = p.x;
      vertex.py = p.y;
      vertex.pz = p.z;
      vertex.pw = 1.0f;
      vertex.nx = normal.x;
      vertex.ny = normal.y;
      vertex.nz = normal.z;
      vertex.nw = 0.0f;
      vertex.u = u;
      vertex.v = v;
      vertices.push_back(vertex);
    };

    glm::vec2 uv0 = make_uv(p0);
    glm::vec2 uv1 = make_uv(p1);
    glm::vec2 uv2 = make_uv(p2);
    glm::vec2 uv3 = make_uv(p3);

    push_vertex(p0, uv0.x, uv0.y);
    push_vertex(p1, uv1.x, uv1.y);
    push_vertex(p2, uv2.x, uv2.y);
    push_vertex(p3, uv3.x, uv3.y);

    indices.push_back(base_vertex + 0);
    indices.push_back(base_vertex + 1);
    indices.push_back(base_vertex + 2);
    indices.push_back(base_vertex + 0);
    indices.push_back(base_vertex + 2);
    indices.push_back(base_vertex + 3);

    glm::vec3 bbox_min = p0;
    glm::vec3 bbox_max = p0;
    bbox_min.x = std::min(bbox_min.x, p1.x);
    bbox_min.y = std::min(bbox_min.y, p1.y);
    bbox_min.z = std::min(bbox_min.z, p1.z);
    bbox_max.x = std::max(bbox_max.x, p1.x);
    bbox_max.y = std::max(bbox_max.y, p1.y);
    bbox_max.z = std::max(bbox_max.z, p1.z);
    bbox_min.x = std::min(bbox_min.x, p2.x);
    bbox_min.y = std::min(bbox_min.y, p2.y);
    bbox_min.z = std::min(bbox_min.z, p2.z);
    bbox_max.x = std::max(bbox_max.x, p2.x);
    bbox_max.y = std::max(bbox_max.y, p2.y);
    bbox_max.z = std::max(bbox_max.z, p2.z);
    bbox_min.x = std::min(bbox_min.x, p3.x);
    bbox_min.y = std::min(bbox_min.y, p3.y);
    bbox_min.z = std::min(bbox_min.z, p3.z);
    bbox_max.x = std::max(bbox_max.x, p3.x);
    bbox_max.y = std::max(bbox_max.y, p3.y);
    bbox_max.z = std::max(bbox_max.z, p3.z);

    SceneObject object;
    object.name = name;
    object.first_index = first_index;
    object.num_indices = 6;
    object.rendering_mode = GL_TRIANGLES;
    object.vertex_array_object_id = vertex_array_object_id;
    object.bbox_min = bbox_min;
    object.bbox_max = bbox_max;
    g_VirtualScene[name] = object;
  };

  const float half_width = kCorridorHalfWidth;
  const float corridor_height = kCorridorHeight;
  const float corridor_length = kCorridorLength;
  const float z0 = kCorridorZ0;
  const float z1 = kCorridorZ1;

  add_quad("corridor_floor", glm::vec3(-half_width, 0.0f, z0),
           glm::vec3(+half_width, 0.0f, z0), glm::vec3(+half_width, 0.0f, z1),
           glm::vec3(-half_width, 0.0f, z1), glm::vec3(0.0f, 1.0f, 0.0f),
           glm::vec3(-half_width, 0.0f, z0), glm::vec3(1.0f, 0.0f, 0.0f),
           glm::vec3(0.0f, 0.0f, -1.0f), kFloorTileSize, kFloorTileSize);

  add_quad(
      "corridor_ceiling", glm::vec3(-half_width, corridor_height, z1),
      glm::vec3(+half_width, corridor_height, z1),
      glm::vec3(+half_width, corridor_height, z0),
      glm::vec3(-half_width, corridor_height, z0), glm::vec3(0.0f, -1.0f, 0.0f),
      glm::vec3(-half_width, corridor_height, z0), glm::vec3(1.0f, 0.0f, 0.0f),
      glm::vec3(0.0f, 0.0f, -1.0f), kCeilingTileSize, kCeilingTileSize);

  add_quad("corridor_wall_left", glm::vec3(-half_width, 0.0f, z0),
           glm::vec3(-half_width, 0.0f, z1),
           glm::vec3(-half_width, corridor_height, z1),
           glm::vec3(-half_width, corridor_height, z0),
           glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(-half_width, 0.0f, z0),
           glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f),
           kWallTextureTileSize, kWallTextureTileSize);

  add_quad("corridor_wall_right", glm::vec3(+half_width, 0.0f, z1),
           glm::vec3(+half_width, 0.0f, z0),
           glm::vec3(+half_width, corridor_height, z0),
           glm::vec3(+half_width, corridor_height, z1),
           glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(+half_width, 0.0f, z0),
           glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f),
           kWallTextureTileSize, kWallTextureTileSize);

  // --- TACTILE PAVING GEOMETRY ---
  const float ty = 0.005f;
  const float thw = 0.15f;
  const float tts = 0.3f;

  add_quad("corridor_tactile_straight_lines", glm::vec3(-thw, ty, z0),
           glm::vec3(thw, ty, z0), glm::vec3(thw, ty, z1),
           glm::vec3(-thw, ty, z1), glm::vec3(0.0f, 1.0f, 0.0f),
           glm::vec3(-thw, ty, z0), glm::vec3(1.0f, 0.0f, 0.0f),
           glm::vec3(0.0f, 0.0f, -1.0f), tts, tts);

  // Generate anomalous tactile paving
  struct TactileVertex {
    float px, py, pz, pw;
    float nx, ny, nz, nw;
    float u, v;
  };
  auto build_tactile_mesh = [&](const std::string& name, const std::vector<glm::vec4>& quads) {
      std::vector<TactileVertex> vertices;
      std::vector<GLuint> indices;
      GLuint vao; glGenVertexArrays(1, &vao); glBindVertexArray(vao);
      
      float local_ty = ty;
      if (name == "corridor_tactile_anomaly_square") local_ty = ty + 0.001f; // Elevate slightly to prevent z-fighting with the straight line
      
      glm::vec3 bbox_min(9999.0f), bbox_max(-9999.0f);
      for(size_t i=0; i<quads.size(); ++i) {
          float cx = quads[i].x; float cz = quads[i].y;
          float hw = quads[i].z; float hl = quads[i].w;
          glm::vec3 p0(cx - hw, local_ty, cz + hl);
          glm::vec3 p1(cx + hw, local_ty, cz + hl);
          glm::vec3 p2(cx + hw, local_ty, cz - hl);
          glm::vec3 p3(cx - hw, local_ty, cz - hl);
          
          auto push_v = [&](glm::vec3 p, float u, float v) {
              vertices.push_back({p.x, p.y, p.z, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v});
              bbox_min = glm::min(bbox_min, p);
              bbox_max = glm::max(bbox_max, p);
          };
          // Map U to world X, V to world -Z for seamless tiling
          push_v(p0, p0.x/tts, -p0.z/tts); 
          push_v(p1, p1.x/tts, -p1.z/tts); 
          push_v(p2, p2.x/tts, -p2.z/tts); 
          push_v(p3, p3.x/tts, -p3.z/tts);
          GLuint base = i * 4;
          indices.push_back(base+0); indices.push_back(base+1); indices.push_back(base+2);
          indices.push_back(base+0); indices.push_back(base+2); indices.push_back(base+3);
      }
      
      GLuint vbo, ebo;
      glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(TactileVertex), vertices.data(), GL_STATIC_DRAW);
      glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(TactileVertex), (void*)0); glEnableVertexAttribArray(0);
      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(TactileVertex), (void*)(4*sizeof(float))); glEnableVertexAttribArray(1);
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TactileVertex), (void*)(8*sizeof(float))); glEnableVertexAttribArray(2);
      glGenBuffers(1, &ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
      glBindVertexArray(vertex_array_object_id);
      
      SceneObject obj; obj.name = name; obj.first_index = 0; obj.num_indices = indices.size();
      obj.rendering_mode = GL_TRIANGLES; obj.vertex_array_object_id = vao;
      obj.bbox_min = bbox_min; obj.bbox_max = bbox_max;
      g_VirtualScene[name] = obj;
  };

  std::vector<glm::vec4> anomaly_lines;
  std::vector<glm::vec4> square_quads;
  
  // Create a 2x2m square frame of dots centered at origin
  // Top and bottom edges
  square_quads.push_back(glm::vec4(0.0f, -1.0f, 1.0f + thw, thw));
  square_quads.push_back(glm::vec4(0.0f, 1.0f, 1.0f + thw, thw));
  // Left and right edges
  square_quads.push_back(glm::vec4(-1.0f, 0.0f, thw, 1.0f - thw));
  square_quads.push_back(glm::vec4(1.0f, 0.0f, thw, 1.0f - thw));
  
  build_tactile_mesh("corridor_tactile_anomaly_square", square_quads);


  const float doorway_y_low = 0.0f;
  const float doorway_y_high = kDoorwayOpeningHeight;
  const float right_wall_x = +half_width;
  const float right_reveal_x = right_wall_x + kDoorwayRecessDepth;
  const float left_wall_x = -half_width;
  const float left_reveal_x = left_wall_x - kDoorwayRecessDepth;

  struct DoorwayOpening {
    int slot;
    float z_low;
    float z_high;
  };

  auto generate_wall_meshes = [&](bool backward) {
    std::string prefix = backward ? "_backward" : "";
    std::vector<DoorwayOpening> doorway_openings;
  doorway_openings.reserve(kDoorwayCount);
  for (int slot = 0; slot < kDoorwayCount; ++slot) {
    float fraction = kDoorwayDistanceFractions[slot];
      if (backward) fraction = 1.0f - fraction;
      const float center_z = z0 - corridor_length * fraction;
    doorway_openings.push_back(
        DoorwayOpening{slot, center_z - 0.5f * kDoorwayOpeningWidth,
                       center_z + 0.5f * kDoorwayOpeningWidth});
  }
  std::sort(doorway_openings.begin(), doorway_openings.end(),
            [](const DoorwayOpening &a, const DoorwayOpening &b) {
              return a.z_low < b.z_low;
            });

  auto add_right_wall_piece = [&](const std::string &name, float z_min,
                                  float z_max, float y_min, float y_max) {
    if (z_max <= z_min || y_max <= y_min)
      return;
    add_quad(name, glm::vec3(right_wall_x, y_min, z_min),
             glm::vec3(right_wall_x, y_min, z_max),
             glm::vec3(right_wall_x, y_max, z_max),
             glm::vec3(right_wall_x, y_max, z_min),
             glm::vec3(-1.0f, 0.0f, 0.0f),
             glm::vec3(right_wall_x, 0.0f, z0),
             glm::vec3(0.0f, 0.0f, -1.0f),
             glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
             kWallTextureTileSize);
  };

  auto add_left_wall_piece = [&](const std::string &name, float z_min,
                                 float z_max, float y_min, float y_max) {
    if (z_max <= z_min || y_max <= y_min)
      return;
    add_quad(name, glm::vec3(left_wall_x, y_min, z_max),
             glm::vec3(left_wall_x, y_min, z_min),
             glm::vec3(left_wall_x, y_max, z_min),
             glm::vec3(left_wall_x, y_max, z_max),
             glm::vec3(1.0f, 0.0f, 0.0f),
             glm::vec3(left_wall_x, 0.0f, z0),
             glm::vec3(0.0f, 0.0f, -1.0f),
             glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
             kWallTextureTileSize);
  };

  float span_start = z1;
  for (int span = 0; span <= kDoorwayCount; ++span) {
    const float span_end =
        (span < kDoorwayCount) ? doorway_openings[span].z_low : z0;
    add_right_wall_piece("corridor_wall_right_doorway" + prefix + "_span_" +
                             std::to_string(span),
                         span_start, span_end, 0.0f, corridor_height);
    add_left_wall_piece("corridor_wall_left_doorway" + prefix + "_span_" +
                            std::to_string(span),
                        span_start, span_end, 0.0f, corridor_height);
    if (span < kDoorwayCount)
      span_start = doorway_openings[span].z_high;
  }

  for (const DoorwayOpening &opening : doorway_openings) {
      const int slot = opening.slot;
      const float z_low = opening.z_low;
      const float z_high = opening.z_high;
      const std::string suffix = prefix + "_" + std::to_string(slot);

    add_right_wall_piece("corridor_wall_right_doorway_fill" + suffix,
                         z_low, z_high, doorway_y_low, corridor_height);
    add_left_wall_piece("corridor_wall_left_doorway_fill" + suffix,
                        z_low, z_high, doorway_y_low, corridor_height);

    add_right_wall_piece("corridor_wall_right_doorway_top" + suffix,
                         z_low, z_high, doorway_y_high, corridor_height);
    add_left_wall_piece("corridor_wall_left_doorway_top" + suffix,
                        z_low, z_high, doorway_y_high, corridor_height);

    add_quad("corridor_wall_right_doorway_reveal_low" + suffix,
             glm::vec3(right_wall_x, doorway_y_low, z_low),
             glm::vec3(right_reveal_x, doorway_y_low, z_low),
             glm::vec3(right_reveal_x, doorway_y_high, z_low),
             glm::vec3(right_wall_x, doorway_y_high, z_low),
             glm::vec3(0.0f, 0.0f, 1.0f),
             glm::vec3(right_reveal_x, doorway_y_low, z_low),
             glm::vec3(1.0f, 0.0f, 0.0f),
             glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
             kWallTextureTileSize);
    add_quad("corridor_wall_right_doorway_reveal_high" + suffix,
             glm::vec3(right_reveal_x, doorway_y_low, z_high),
             glm::vec3(right_wall_x, doorway_y_low, z_high),
             glm::vec3(right_wall_x, doorway_y_high, z_high),
             glm::vec3(right_reveal_x, doorway_y_high, z_high),
             glm::vec3(0.0f, 0.0f, -1.0f),
             glm::vec3(right_reveal_x, doorway_y_low, z_high),
             glm::vec3(1.0f, 0.0f, 0.0f),
             glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
             kWallTextureTileSize);
    add_quad("corridor_wall_right_doorway_reveal_top" + suffix,
             glm::vec3(right_wall_x, doorway_y_high, z_low),
             glm::vec3(right_reveal_x, doorway_y_high, z_low),
             glm::vec3(right_reveal_x, doorway_y_high, z_high),
             glm::vec3(right_wall_x, doorway_y_high, z_high),
             glm::vec3(0.0f, -1.0f, 0.0f),
             glm::vec3(right_reveal_x, doorway_y_high, z_low),
             glm::vec3(1.0f, 0.0f, 0.0f),
             glm::vec3(0.0f, 0.0f, 1.0f), kWallTextureTileSize,
             kWallTextureTileSize);

    add_quad("corridor_wall_left_doorway_reveal_low" + suffix,
             glm::vec3(left_reveal_x, doorway_y_low, z_low),
             glm::vec3(left_wall_x, doorway_y_low, z_low),
             glm::vec3(left_wall_x, doorway_y_high, z_low),
             glm::vec3(left_reveal_x, doorway_y_high, z_low),
             glm::vec3(0.0f, 0.0f, 1.0f),
             glm::vec3(left_wall_x, doorway_y_low, z_low),
             glm::vec3(1.0f, 0.0f, 0.0f),
             glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
             kWallTextureTileSize);
    add_quad("corridor_wall_left_doorway_reveal_high" + suffix,
             glm::vec3(left_wall_x, doorway_y_low, z_high),
             glm::vec3(left_reveal_x, doorway_y_low, z_high),
             glm::vec3(left_reveal_x, doorway_y_high, z_high),
             glm::vec3(left_wall_x, doorway_y_high, z_high),
             glm::vec3(0.0f, 0.0f, -1.0f),
             glm::vec3(left_wall_x, doorway_y_low, z_high),
             glm::vec3(1.0f, 0.0f, 0.0f),
             glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
             kWallTextureTileSize);
    add_quad("corridor_wall_left_doorway_reveal_top" + suffix,
             glm::vec3(left_reveal_x, doorway_y_high, z_low),
             glm::vec3(left_wall_x, doorway_y_high, z_low),
             glm::vec3(left_wall_x, doorway_y_high, z_high),
             glm::vec3(left_reveal_x, doorway_y_high, z_high),
             glm::vec3(0.0f, -1.0f, 0.0f),
             glm::vec3(left_wall_x, doorway_y_high, z_low),
             glm::vec3(1.0f, 0.0f, 0.0f),
             glm::vec3(0.0f, 0.0f, 1.0f), kWallTextureTileSize,
             kWallTextureTileSize);
    }
  };

  generate_wall_meshes(false);
  generate_wall_meshes(true);

  GLuint vertex_buffer_id;
  glGenBuffers(1, &vertex_buffer_id);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(CorridorVertex),
               vertices.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(CorridorVertex),
                        (void *)offsetof(CorridorVertex, px));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(CorridorVertex),
                        (void *)offsetof(CorridorVertex, nx));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(CorridorVertex),
                        (void *)offsetof(CorridorVertex, u));
  glEnableVertexAttribArray(2);

  GLuint index_buffer_id;
  glGenBuffers(1, &index_buffer_id);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_id);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint),
               indices.data(), GL_STATIC_DRAW);

  glBindVertexArray(0);
}

void BuildDoorwayPlaceholderAndAddToVirtualScene() {
  struct PlaceholderVertex {
    float px, py, pz, pw;
    float nx, ny, nz, nw;
    float u, v;
  };

  const PlaceholderVertex vertices[] = {
      {0.0f, 0.0f, +0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, -0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f},
      {0.0f, 1.0f, -0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f},
      {0.0f, 1.0f, +0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
  };
  const GLuint indices[] = {0, 1, 2, 0, 2, 3};

  GLuint vertex_array_object_id;
  glGenVertexArrays(1, &vertex_array_object_id);
  glBindVertexArray(vertex_array_object_id);

  GLuint vertex_buffer_id;
  glGenBuffers(1, &vertex_buffer_id);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(PlaceholderVertex),
                        (void *)offsetof(PlaceholderVertex, px));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(PlaceholderVertex),
                        (void *)offsetof(PlaceholderVertex, nx));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(PlaceholderVertex),
                        (void *)offsetof(PlaceholderVertex, u));
  glEnableVertexAttribArray(2);

  GLuint index_buffer_id;
  glGenBuffers(1, &index_buffer_id);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_id);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
               GL_STATIC_DRAW);

  SceneObject object;
  object.name = "doorway_placeholder_panel";
  object.first_index = 0;
  object.num_indices = 6;
  object.rendering_mode = GL_TRIANGLES;
  object.vertex_array_object_id = vertex_array_object_id;
  object.bbox_min = glm::vec3(0.0f, 0.0f, -0.5f);
  object.bbox_max = glm::vec3(0.0f, 1.0f, 0.5f);
  g_VirtualScene[object.name] = object;

  glBindVertexArray(0);
}

void BuildCornerAndAddToVirtualScene() {
  struct CornerVertex {
    float px, py, pz, pw;
    float nx, ny, nz, nw;
    float u, v;
  };

  // Agora recebemos 4 booleanos, um para cada parede possível!
  auto build_corner_parts = [&](const std::string &prefix, bool wall_front,
                                bool wall_back, bool wall_left,
                                bool wall_right) {
    auto add_quad = [&](const std::string &name, glm::vec3 p0, glm::vec3 p1,
                        glm::vec3 p2, glm::vec3 p3, glm::vec3 normal,
                        glm::vec3 uv_origin, glm::vec3 uv_u_axis,
                        glm::vec3 uv_v_axis, float u_tile_size,
                        float v_tile_size) {
      std::vector<CornerVertex> vertices;
      std::vector<GLuint> indices = {0, 1, 2, 0, 2, 3};

      auto make_uv = [&](const glm::vec3 &p) {
        glm::vec3 u_axis = glm::normalize(uv_u_axis);
        glm::vec3 v_axis = glm::normalize(uv_v_axis);
        glm::vec3 delta = p - uv_origin;
        float u = glm::dot(delta, u_axis);
        float v = glm::dot(delta, v_axis);
        return glm::vec2(u / u_tile_size, v / v_tile_size);
      };

      glm::vec2 uv0 = make_uv(p0);
      glm::vec2 uv1 = make_uv(p1);
      glm::vec2 uv2 = make_uv(p2);
      glm::vec2 uv3 = make_uv(p3);

      vertices.push_back({p0.x, p0.y, p0.z, 1.0f, normal.x, normal.y, normal.z,
                          0.0f, uv0.x, uv0.y});
      vertices.push_back({p1.x, p1.y, p1.z, 1.0f, normal.x, normal.y, normal.z,
                          0.0f, uv1.x, uv1.y});
      vertices.push_back({p2.x, p2.y, p2.z, 1.0f, normal.x, normal.y, normal.z,
                          0.0f, uv2.x, uv2.y});
      vertices.push_back({p3.x, p3.y, p3.z, 1.0f, normal.x, normal.y, normal.z,
                          0.0f, uv3.x, uv3.y});

      GLuint vao, vbo, ebo;
      glGenVertexArrays(1, &vao);
      glBindVertexArray(vao);
      glGenBuffers(1, &vbo);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(CornerVertex),
                   vertices.data(), GL_STATIC_DRAW);
      glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(CornerVertex),
                            (void *)offsetof(CornerVertex, px));
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(CornerVertex),
                            (void *)offsetof(CornerVertex, nx));
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(CornerVertex),
                            (void *)offsetof(CornerVertex, u));
      glEnableVertexAttribArray(2);
      glGenBuffers(1, &ebo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint),
                   indices.data(), GL_STATIC_DRAW);
      glBindVertexArray(0);

      SceneObject object;
      object.name = name;
      object.first_index = 0;
      object.num_indices = 6;
      object.rendering_mode = GL_TRIANGLES;
      object.vertex_array_object_id = vao;
      object.bbox_min = p3;
      object.bbox_max = p1;
      g_VirtualScene[name] = object;
    };

    const float hw = kCorridorHalfWidth;
    const float h = kCorridorHeight;
    const float z0 = 0.0f;
    const float z1 = -kCornerLength;

    // Chão e Teto
    add_quad(prefix + "_floor", glm::vec3(-hw, 0.0f, z0),
             glm::vec3(hw, 0.0f, z0), glm::vec3(hw, 0.0f, z1),
             glm::vec3(-hw, 0.0f, z1), glm::vec3(0.0f, 1.0f, 0.0f),
             glm::vec3(-hw, 0.0f, z0), glm::vec3(1.0f, 0.0f, 0.0f),
             glm::vec3(0.0f, 0.0f, -1.0f), kFloorTileSize, kFloorTileSize);
    add_quad(prefix + "_ceiling", glm::vec3(-hw, h, z1), glm::vec3(hw, h, z1),
             glm::vec3(hw, h, z0), glm::vec3(-hw, h, z0),
             glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(-hw, h, z0),
             glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
             kCeilingTileSize, kCeilingTileSize);

    // Paredes dinâmicas (só cria se for TRUE)
    if (wall_front)
      add_quad(prefix + "_wall_front", glm::vec3(hw, 0.0f, z0),
               glm::vec3(-hw, 0.0f, z0), glm::vec3(-hw, h, z0),
               glm::vec3(hw, h, z0), glm::vec3(0.0f, 0.0f, -1.0f),
               glm::vec3(hw, 0.0f, z0), glm::vec3(-1.0f, 0.0f, 0.0f),
               glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
               kWallTextureTileSize);
    if (wall_back)
      add_quad(prefix + "_wall_back", glm::vec3(-hw, 0.0f, z1),
               glm::vec3(hw, 0.0f, z1), glm::vec3(hw, h, z1),
               glm::vec3(-hw, h, z1), glm::vec3(0.0f, 0.0f, 1.0f),
               glm::vec3(-hw, 0.0f, z1), glm::vec3(1.0f, 0.0f, 0.0f),
               glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
               kWallTextureTileSize);
    if (wall_left)
      add_quad(prefix + "_wall_left", glm::vec3(-hw, 0.0f, z0),
               glm::vec3(-hw, 0.0f, z1), glm::vec3(-hw, h, z1),
               glm::vec3(-hw, h, z0), glm::vec3(1.0f, 0.0f, 0.0f),
               glm::vec3(-hw, 0.0f, z0), glm::vec3(0.0f, 0.0f, -1.0f),
               glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
               kWallTextureTileSize);
    if (wall_right)
      add_quad(prefix + "_wall_right", glm::vec3(hw, 0.0f, z1),
               glm::vec3(hw, 0.0f, z0), glm::vec3(hw, h, z0),
               glm::vec3(hw, h, z1), glm::vec3(-1.0f, 0.0f, 0.0f),
               glm::vec3(hw, 0.0f, z0), glm::vec3(0.0f, 0.0f, -1.0f),
               glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
               kWallTextureTileSize);

    // --- TACTILE PAVING CORNERS ---
    const float ty = 0.005f;
    const float thw = 0.15f;
    const float tts = 0.3f;
    const float center_z = z1 / 2.0f; // -2.0f

    // Center dot square
    add_quad(prefix + "_tactile_dots", glm::vec3(-thw, ty, center_z + thw),
             glm::vec3(thw, ty, center_z + thw), glm::vec3(thw, ty, center_z - thw),
             glm::vec3(-thw, ty, center_z - thw), glm::vec3(0.0f, 1.0f, 0.0f),
             glm::vec3(-thw, ty, center_z + thw), glm::vec3(1.0f, 0.0f, 0.0f),
             glm::vec3(0.0f, 0.0f, -1.0f), tts, tts);

    // Line from Z axis to center
    if (prefix == "corner_left") {
        add_quad(prefix + "_tactile_line_in", glm::vec3(-thw, ty, z0),
                 glm::vec3(thw, ty, z0), glm::vec3(thw, ty, center_z + thw),
                 glm::vec3(-thw, ty, center_z + thw), glm::vec3(0.0f, 1.0f, 0.0f),
                 glm::vec3(-thw, ty, z0), glm::vec3(1.0f, 0.0f, 0.0f),
                 glm::vec3(0.0f, 0.0f, -1.0f), tts, tts);
    } else if (prefix == "corner_right") {
        // Line from center to z1 (starts new corridor)
        add_quad(prefix + "_tactile_line_in", glm::vec3(-thw, ty, center_z - thw),
                 glm::vec3(thw, ty, center_z - thw), glm::vec3(thw, ty, z1),
                 glm::vec3(-thw, ty, z1), glm::vec3(0.0f, 1.0f, 0.0f),
                 glm::vec3(-thw, ty, center_z - thw), glm::vec3(1.0f, 0.0f, 0.0f),
                 glm::vec3(0.0f, 0.0f, -1.0f), tts, tts);
    }

    // Line from center to left or right connector
    if (prefix == "corner_left") {
        add_quad(prefix + "_tactile_line_out", glm::vec3(-thw, ty, center_z + thw),
                 glm::vec3(-thw, ty, center_z - thw), glm::vec3(-hw, ty, center_z - thw),
                 glm::vec3(-hw, ty, center_z + thw), glm::vec3(0.0f, 1.0f, 0.0f),
                 glm::vec3(-hw, ty, center_z + thw), glm::vec3(0.0f, 0.0f, -1.0f),
                 glm::vec3(-1.0f, 0.0f, 0.0f), tts, tts);
    } else if (prefix == "corner_right") {
        add_quad(prefix + "_tactile_line_out", glm::vec3(hw, ty, center_z + thw),
                 glm::vec3(hw, ty, center_z - thw), glm::vec3(thw, ty, center_z - thw),
                 glm::vec3(thw, ty, center_z + thw), glm::vec3(0.0f, 1.0f, 0.0f),
                 glm::vec3(thw, ty, center_z + thw), glm::vec3(0.0f, 0.0f, -1.0f),
                 glm::vec3(-1.0f, 0.0f, 0.0f), tts, tts);
    }
  };

  // Quina 1 (Vira à esquerda): Aberta na frente (para o Corredor 1) e aberta
  // na esquerda (para o Conector). Tem parede no fundo e na direita.
  build_corner_parts("corner_left", false, true, false, true);

  // Quina 2 (Vira à direita): Aberta na direita (vindo do Conector) e aberta
  // no fundo (para o Corredor 2). Tem parede na frente e na esquerda.
  build_corner_parts("corner_right", true, false, true, false);
}
