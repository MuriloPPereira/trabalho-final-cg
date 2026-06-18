#include "world/TeleportSystem.h"

#include "entities/CamouflagedPursuer.h"
#include "entities/NPC.h"
#include "world/Corridor.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

void UpdateTeleportSystem(const CollisionResult &col, glm::vec4 &camera_position) {
  const CanonicalCorridorLayout corridor_layout = GetCanonicalCorridorLayout();

  glm::vec2 p_world = col.p_world;
  int block_index = col.block_index;
  bool inside_straight_corridor = col.inside_straight_corridor;
  bool inside_connector_turn = col.inside_connector_turn;
  float connector_progress = col.connector_progress;

  const int player_section = inside_connector_turn ? 1 : 0;
  if (player_section != g_LastPlayerSection) {
    g_LastPlayerSection = player_section;
  }

  auto candidate_for_direction =
      [&](int transition_direction) -> const CorridorInstance & {
    return (transition_direction > 0) ? g_PositiveCandidateCorridorInstance
                                      : g_NegativeCandidateCorridorInstance;
  };

  auto log_connector_enter = [&](int transition_direction) {
    if (transition_direction == 0)
      return;

    g_PreparedTransitionDirection = transition_direction;
    g_PreparedNextCorridorId = -1;
  };

  if (inside_connector_turn) {
    if (!g_InConnectorTransition) {
      g_InConnectorTransition = true;
      g_ConnectorMidpointCrossed = false;
      const int transition_direction = (block_index < 0) ? -1 : +1;
      log_connector_enter(transition_direction);
    }

    const int transition_direction = (g_PreparedTransitionDirection == 0)
                                         ? ((block_index < 0) ? -1 : +1)
                                         : g_PreparedTransitionDirection;
    const bool midpoint_crossed_now = (transition_direction > 0)
                                          ? (connector_progress >= 0.5f)
                                          : (connector_progress <= 0.5f);

    if (!g_ConnectorMidpointCrossed && midpoint_crossed_now) {
      g_PreparedNextCorridorId =
          candidate_for_direction(transition_direction).state.id;
      g_ConnectorMidpointCrossed = true;

      ActivateNewLogicalCorridor(transition_direction);
      if (transition_direction > 0)
        p_world -= corridor_layout.block_offset;
      else
        p_world += corridor_layout.block_offset;

      const glm::vec3 player_position(p_world.x, camera_position.y, p_world.y);
      TrySpawnSalarymanForCorridorContent(g_CurrentCorridorInstance.content,
                                          player_position,
                                          "connector_midpoint");
      ActivateCamouflagedPursuerForCorridor(
          g_CamouflagedPursuer, g_CurrentCorridorInstance.content);
      LogCorridorTransition("connector_midpoint", transition_direction,
                            g_CurrentCorridorInstance.content, player_position);
      g_PreparedNextCorridorId = -1;
    }
  } else if (inside_straight_corridor && g_InConnectorTransition) {
    g_InConnectorTransition = false;
    g_PreparedNextCorridorId = -1;
    g_PreparedTransitionDirection = 0;
    g_ConnectorMidpointCrossed = false;
  }

  camera_position.x = p_world.x;
  camera_position.z = p_world.y;
  camera_position.w = 1.0f;
}
