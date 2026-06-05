#ifndef WORLD_CORRIDOR_H
#define WORLD_CORRIDOR_H

#include "collisions.h"
#include "utils/Constants.h"
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

struct CorridorState { int id; bool has_anomaly; };
struct CorridorRenderTransform { glm::mat4 geometryFromLocal; };
struct CorridorContentFrame {
  int logicalCorridorId;
  glm::vec3 contentOrigin;
  glm::vec3 contentForward;
  glm::vec3 contentRight;
  float corridorLength;
  float connectorLength;
  const char *posterWallSide;
};
struct PosterSlotLayout {
  int slot;
  int textureIndex;
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 up;
  glm::vec3 widthAxis;
  const char *wallSide;
};
struct CorridorContent {
  int corridorId;
  CorridorContentFrame frame;
  std::vector<PosterSlotLayout> posters;
  std::vector<glm::vec3> lightPositions;
  glm::vec3 salarymanSpawnPosition;
  glm::vec3 salarymanForward;
  bool hasAnomaly;
};
struct CorridorInstance { CorridorState state; CorridorContent content; };

extern int g_CurrentExitLevel;
extern int g_CurrentCorridorSequenceId;
extern int g_NextCorridorSequenceId;
extern int g_LastEnteredPhysicalSide;
extern int g_LastSalarymanSpawnCorridorId;
extern int g_PreparedNextCorridorId;
extern int g_PreparedTransitionDirection;
extern bool g_InConnectorTransition;
extern bool g_ConnectorMidpointCrossed;
extern int g_LastPlayerSection;
extern CorridorInstance g_CurrentCorridorInstance;
extern CorridorInstance g_NegativeCandidateCorridorInstance;
extern CorridorInstance g_PositiveCandidateCorridorInstance;

int PositiveModulo(int value, int divisor);
int GetPosterTextureIndex(int corridor_id, int poster_slot);
CorridorState MakeCorridorState(int id);
void RefreshCandidateCorridorStates();
void InitializeCorridorLifecycle();
void ActivateNewLogicalCorridor(int physical_side);
CanonicalCorridorLayout GetCanonicalCorridorLayout();
glm::vec3 TransformPoint(const glm::mat4 &transform, const glm::vec3 &point);
glm::vec3 TransformVector(const glm::mat4 &transform, const glm::vec3 &vector);
CorridorRenderTransform MakeCorridorRenderTransform(const glm::mat4 &geometry_from_local);
CorridorContentFrame MakeCorridorContentFrame(int logical_corridor_id, const glm::vec3 &requested_content_forward);
CorridorContent GenerateCorridorContent(int corridor_id, const glm::vec3 &content_forward, bool has_anomaly);
CorridorInstance CreateNewCorridorInstance(int logical_id, const glm::vec3 &content_forward, bool has_anomaly);
std::string PosterOrderString(const CorridorContent &content);
void LogCorridorContentSignature(const char *reason, int render_slot, int traversal_direction, const CorridorInstance &corridor_instance, const glm::vec3 &block_display_offset);
void LogCorridorSlotStability(const char *reason, int traversal_direction, const CorridorInstance &corridor_instance);
void LogCorridorWindow(const char *reason, int traversal_direction);
void LogCorridorTransition(const char *reason, int traversal_direction, const CorridorContent &content, const glm::vec3 &player_position);
glm::vec3 ComputeSalarymanSpawnPosition(const CorridorContentFrame &frame);
const char *PlayerSectionName(int player_section);
void BuildCorridorAndAddToVirtualScene();
void BuildCornerAndAddToVirtualScene();
void BuildPostersAndAddToVirtualScene();

#endif
