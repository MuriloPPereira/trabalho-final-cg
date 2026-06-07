#include "world/Corridor.h"

#include "engine/Renderer.h"
#include "entities/NPC.h"
#include "matrices.h"
#include "rendering/Mesh.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

int g_CurrentExitLevel = 0;
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

int PositiveModulo(int value, int divisor) {
  int result = value % divisor;
  return (result < 0) ? result + divisor : result;
}

int GetPosterTextureIndex(int corridor_id, int poster_slot) {
  (void)corridor_id;
  return PositiveModulo(poster_slot, kPosterCount);
}

CorridorContent GenerateCorridorContent(int corridor_id,
                                        const glm::vec3 &content_forward,
                                        bool has_anomaly);
CorridorInstance CreateNewCorridorInstance(int logical_id,
                                           const glm::vec3 &content_forward,
                                           bool has_anomaly);

CorridorState MakeCorridorState(int id) {
  CorridorState state;
  state.id = id;
  state.has_anomaly = false; // Hook for later anomaly selection.
  return state;
}

void RefreshCandidateCorridorStates() {

  bool next_has_anomaly = (rand() % 2 == 0);

  g_NegativeCandidateCorridorInstance = CreateNewCorridorInstance(
      g_NextCorridorSequenceId, glm::vec3(0.0f, 0.0f, +1.0f), next_has_anomaly);
  g_PositiveCandidateCorridorInstance = CreateNewCorridorInstance(
      g_NextCorridorSequenceId, glm::vec3(0.0f, 0.0f, -1.0f), next_has_anomaly);

  g_NegativeCandidateCorridorInstance.state.has_anomaly = next_has_anomaly;
  g_NegativeCandidateCorridorInstance.content.hasAnomaly = next_has_anomaly;

  g_PositiveCandidateCorridorInstance.state.has_anomaly = next_has_anomaly;
  g_PositiveCandidateCorridorInstance.content.hasAnomaly = next_has_anomaly;
}
void InitializeCorridorLifecycle() {
  g_CurrentCorridorSequenceId = 0;
  g_NextCorridorSequenceId = 1;
  g_CurrentCorridorInstance = CreateNewCorridorInstance(
      g_CurrentCorridorSequenceId, glm::vec3(0.0f, 0.0f, -1.0f), false);
  RefreshCandidateCorridorStates();
}

void ActivateNewLogicalCorridor(int physical_side) {
  if (physical_side == 0)
    return;


  float player_moved_z = -(float)physical_side;

  // O jogador seguiu em frente se ele andou na mesma direção que a frente do corredor aponta
  bool went_forward = (player_moved_z == g_CurrentCorridorInstance.content.frame.contentForward.z);
  bool had_anomaly = g_CurrentCorridorInstance.state.has_anomaly;

  bool is_correct =
      (went_forward && !had_anomaly) || (!went_forward && had_anomaly);

  if (is_correct) {
    g_CurrentExitLevel++;
    printf("\n--- CORRECT -> EXIT LEVEL: %d ---\n\n", g_CurrentExitLevel);
  } else {
    g_CurrentExitLevel = 0;
    printf("\n--- INCORRECT -> EXIT LEVEL: %d ---\n\n", g_CurrentExitLevel);
  }

  if (g_CurrentExitLevel >= 8)
    printf("\n*** YOU WIN! You reached Exit 8! ***\n\n");

  fflush(stdout);

  g_LastEnteredPhysicalSide = physical_side;
  g_CurrentCorridorInstance = (physical_side < 0)
                                  ? g_NegativeCandidateCorridorInstance
                                  : g_PositiveCandidateCorridorInstance;

  if (g_CurrentCorridorInstance.state.has_anomaly) {
    printf("\n[SPOILER] ANOMALIA NO CORREDOR ATUAL (POSTERS IDENTICOS)\n");
  } else {
    printf("\n[SPOILER] CORREDOR NORMAL\n");
  }
  fflush(stdout);
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
                                        bool has_anomaly) {
  const float poster_center_y = 1.6f;
  const float poster_offset = 0.02f;
  const float poster_wall_offset = kCorridorHalfWidth - poster_offset;
  const float spacing = kCorridorLength / (kPosterCount + 1);
  const float salaryman_spawn_distance = kCorridorLength * 0.70f;
  const float salaryman_end_margin = 6.0f;

  CorridorContent content;
  content.corridorId = corridor_id;
  content.frame = MakeCorridorContentFrame(corridor_id, content_forward);
  content.posters.reserve(kPosterCount);
  content.doorways.reserve(kDoorwayCount);
  content.lightPositions.reserve(7);
  content.hasAnomaly = false;

  const CorridorContentFrame &frame = content.frame;
  for (int slot = 0; slot < kPosterCount; ++slot) {
    const float poster_distance = (slot + 1) * spacing;

    PosterSlotLayout poster;
    poster.slot = slot;
    if (has_anomaly) {
      poster.textureIndex = 0;
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

  for (int slot = 0; slot < kDoorwayCount; ++slot) {
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
  const float clamped_spawn_distance =
      std::max(0.0f, std::min(salaryman_spawn_distance,
                              frame.corridorLength - salaryman_end_margin));
  content.salarymanSpawnPosition =
      frame.contentOrigin + frame.contentForward * clamped_spawn_distance;
  content.salarymanSpawnPosition.y = 0.0f;

  return content;
}

CorridorInstance CreateNewCorridorInstance(int logical_id,
                                           const glm::vec3 &content_forward,
                                           bool has_anomaly) {
  CorridorInstance instance;
  instance.state = MakeCorridorState(logical_id);
  instance.state.has_anomaly = has_anomaly;
  instance.content =
      GenerateCorridorContent(logical_id, content_forward, has_anomaly);
  return instance;
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
  const float salaryman_spawn_distance = frame.corridorLength * 0.70f;
  const float salaryman_end_margin = 6.0f;

  glm::vec3 forward = frame.contentForward;
  if (glm::length(forward) < 0.0001f)
    forward = glm::vec3(0.0f, 0.0f, -1.0f);
  else
    forward = glm::normalize(forward);

  const float clamped_spawn_distance =
      std::max(0.0f, std::min(salaryman_spawn_distance,
                              frame.corridorLength - salaryman_end_margin));
  glm::vec3 spawn_position =
      frame.contentOrigin + forward * clamped_spawn_distance;
  spawn_position.y = 0.0f;
  return spawn_position;
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

  const float doorway_y_low = 0.0f;
  const float doorway_y_high = kDoorwayOpeningHeight;
  const float right_wall_x = +half_width;
  const float right_reveal_x = right_wall_x + kDoorwayRecessDepth;
  const float left_wall_x = -half_width;
  const float left_reveal_x = left_wall_x - kDoorwayRecessDepth;

  struct DoorwayOpening {
    float z_low;
    float z_high;
  };

  std::vector<DoorwayOpening> doorway_openings;
  doorway_openings.reserve(kDoorwayCount);
  for (int slot = 0; slot < kDoorwayCount; ++slot) {
    const float center_z =
        z0 - corridor_length * kDoorwayDistanceFractions[slot];
    doorway_openings.push_back(
        DoorwayOpening{center_z - 0.5f * kDoorwayOpeningWidth,
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
    add_right_wall_piece("corridor_wall_right_doorway_span_" +
                             std::to_string(span),
                         span_start, span_end, 0.0f, corridor_height);
    add_left_wall_piece("corridor_wall_left_doorway_span_" +
                            std::to_string(span),
                        span_start, span_end, 0.0f, corridor_height);
    if (span < kDoorwayCount)
      span_start = doorway_openings[span].z_high;
  }

  for (int slot = 0; slot < kDoorwayCount; ++slot) {
    const DoorwayOpening &opening = doorway_openings[slot];
    const float z_low = opening.z_low;
    const float z_high = opening.z_high;

    add_right_wall_piece("corridor_wall_right_doorway_top_" +
                             std::to_string(slot),
                         z_low, z_high, doorway_y_high, corridor_height);
    add_left_wall_piece("corridor_wall_left_doorway_top_" +
                            std::to_string(slot),
                        z_low, z_high, doorway_y_high, corridor_height);

    add_quad("corridor_wall_right_doorway_reveal_low_" +
                 std::to_string(slot),
             glm::vec3(right_wall_x, doorway_y_low, z_low),
             glm::vec3(right_reveal_x, doorway_y_low, z_low),
             glm::vec3(right_reveal_x, doorway_y_high, z_low),
             glm::vec3(right_wall_x, doorway_y_high, z_low),
             glm::vec3(0.0f, 0.0f, 1.0f),
             glm::vec3(right_reveal_x, doorway_y_low, z_low),
             glm::vec3(1.0f, 0.0f, 0.0f),
             glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
             kWallTextureTileSize);
    add_quad("corridor_wall_right_doorway_reveal_high_" +
                 std::to_string(slot),
             glm::vec3(right_reveal_x, doorway_y_low, z_high),
             glm::vec3(right_wall_x, doorway_y_low, z_high),
             glm::vec3(right_wall_x, doorway_y_high, z_high),
             glm::vec3(right_reveal_x, doorway_y_high, z_high),
             glm::vec3(0.0f, 0.0f, -1.0f),
             glm::vec3(right_reveal_x, doorway_y_low, z_high),
             glm::vec3(1.0f, 0.0f, 0.0f),
             glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
             kWallTextureTileSize);
    add_quad("corridor_wall_right_doorway_reveal_top_" +
                 std::to_string(slot),
             glm::vec3(right_wall_x, doorway_y_high, z_low),
             glm::vec3(right_reveal_x, doorway_y_high, z_low),
             glm::vec3(right_reveal_x, doorway_y_high, z_high),
             glm::vec3(right_wall_x, doorway_y_high, z_high),
             glm::vec3(0.0f, -1.0f, 0.0f),
             glm::vec3(right_reveal_x, doorway_y_high, z_low),
             glm::vec3(1.0f, 0.0f, 0.0f),
             glm::vec3(0.0f, 0.0f, 1.0f), kWallTextureTileSize,
             kWallTextureTileSize);

    add_quad("corridor_wall_left_doorway_reveal_low_" +
                 std::to_string(slot),
             glm::vec3(left_reveal_x, doorway_y_low, z_low),
             glm::vec3(left_wall_x, doorway_y_low, z_low),
             glm::vec3(left_wall_x, doorway_y_high, z_low),
             glm::vec3(left_reveal_x, doorway_y_high, z_low),
             glm::vec3(0.0f, 0.0f, 1.0f),
             glm::vec3(left_wall_x, doorway_y_low, z_low),
             glm::vec3(1.0f, 0.0f, 0.0f),
             glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
             kWallTextureTileSize);
    add_quad("corridor_wall_left_doorway_reveal_high_" +
                 std::to_string(slot),
             glm::vec3(left_wall_x, doorway_y_low, z_high),
             glm::vec3(left_reveal_x, doorway_y_low, z_high),
             glm::vec3(left_reveal_x, doorway_y_high, z_high),
             glm::vec3(left_wall_x, doorway_y_high, z_high),
             glm::vec3(0.0f, 0.0f, -1.0f),
             glm::vec3(left_wall_x, doorway_y_low, z_high),
             glm::vec3(1.0f, 0.0f, 0.0f),
             glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
             kWallTextureTileSize);
    add_quad("corridor_wall_left_doorway_reveal_top_" +
                 std::to_string(slot),
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
  };

  // Quina 1 (Vira à esquerda): Aberta na frente (para o Corredor 1) e aberta
  // na esquerda (para o Conector). Tem parede no fundo e na direita.
  build_corner_parts("corner_left", false, true, false, true);

  // Quina 2 (Vira à direita): Aberta na direita (vindo do Conector) e aberta
  // no fundo (para o Corredor 2). Tem parede na frente e na esquerda.
  build_corner_parts("corner_right", true, false, true, false);
}
