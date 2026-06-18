#include "entities/CamouflagedPursuer.h"

#include "engine/Renderer.h"
#include "engine/Shader.h"
#include "entities/Player.h"
#include "matrices.h"
#include "utils/Constants.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

CamouflagedPursuerState g_CamouflagedPursuer;

CamouflagedPursuerState::CamouflagedPursuerState()
    : active(false), visible(false), chasing(false), corridorId(-1),
      triggerRadius(kCamouflagedPursuerTriggerRadius), movementSpeed(0.0f),
      position(0.0f),
      forward(0.0f, 0.0f, -1.0f), placeholderModel(NULL) {}

void ResetCamouflagedPursuer(CamouflagedPursuerState &pursuer) {
  pursuer.active = false;
  pursuer.visible = false;
  pursuer.chasing = false;
  pursuer.corridorId = -1;
  pursuer.triggerRadius = kCamouflagedPursuerTriggerRadius;
  pursuer.movementSpeed = 0.0f;
  pursuer.position = glm::vec3(0.0f);
  pursuer.forward = glm::vec3(0.0f, 0.0f, -1.0f);
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
  }

  if (distance_to_player <= kCamouflagedPursuerStopDistance)
    return;

  pursuer.forward = to_player / distance_to_player;
  pursuer.movementSpeed = GetPlayerThirdPersonShiftSprintSpeed();
  const float remaining_distance =
      distance_to_player - kCamouflagedPursuerStopDistance;
  const float movement_distance =
      std::min(pursuer.movementSpeed * delta_time, remaining_distance);
  pursuer.position += pursuer.forward * movement_distance;
}

void DrawCamouflagedPursuer(const CamouflagedPursuerState &pursuer,
                            const Material &material) {
  if (!pursuer.active || !pursuer.visible || pursuer.placeholderModel == NULL ||
      !pursuer.placeholderModel->loaded)
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
  const glm::vec3 p = pursuer.position;
  const glm::mat4 model_matrix =
      Matrix(right.x, corrected_up.x, forward.x, p.x, right.y, corrected_up.y,
             forward.y, p.y, right.z, corrected_up.z, forward.z, p.z, 0.0f,
             0.0f, 0.0f, 1.0f);

  ApplyMaterial(material);
  glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                     glm::value_ptr(model_matrix));

  // TODO(camouflaged-pursuer-fbx): replace this static debug silhouette with
  // the future dedicated animated FBX model and its independent animator.
  DrawStaticModel(*pursuer.placeholderModel);
}
