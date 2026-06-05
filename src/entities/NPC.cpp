#include "entities/NPC.h"

#include "engine/Renderer.h"
#include "engine/Shader.h"
#include "matrices.h"
#include "utils/Constants.h"

#include <algorithm>
#include <cstdio>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

StaticModel g_SalarymanStaticModel;
SalarymanAnimatedModel g_SalarymanAnimatedModel;
SalarymanAnimator g_SalarymanAnimator;
SalarymanNPC g_SalarymanNPC;

SalarymanNPC::SalarymanNPC()
    : active(false), corridorId(-1), position(0.0f, 0.0f, 0.0f),
      forward(0.0f, 0.0f, -1.0f), speed(1.35f), corridorLength(0.0f),
      corridorOrigin(0.0f, 0.0f, 0.0f), useAnimation(false), model(NULL),
      animatedModel(NULL), animator(NULL) {}

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
  salaryman.position = spawn_position;
  salaryman.forward = path_forward;
  salaryman.speed = 1.35f;
  salaryman.corridorLength = frame.corridorLength;
  salaryman.corridorOrigin =
      frame.contentOrigin - path_forward * frame.corridorLength;
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

  salaryman.position += salaryman.forward * salaryman.speed * delta_time;

  const float corridor_progress = glm::dot(
      salaryman.position - salaryman.corridorOrigin, salaryman.forward);
  const glm::vec3 camera_position(camera_position_c.x, camera_position_c.y,
                                  camera_position_c.z);
  const float player_distance =
      glm::length(salaryman.position - camera_position);

  if (corridor_progress < -2.0f) {
    if (kCorridorDebugLogsEnabled) {
      printf("Salaryman despawn: reason=behind_spawn corridorId=%d "
             "progress=%.2f "
             "playerDistance=%.2f position=(%.2f, %.2f, %.2f), active=false\n",
             salaryman.corridorId, corridor_progress, player_distance,
             salaryman.position.x, salaryman.position.y, salaryman.position.z);
    }
    salaryman.active = false;
  } else if (corridor_progress > salaryman.corridorLength + 2.0f) {
    if (kCorridorDebugLogsEnabled) {
      printf("Salaryman despawn: reason=exited_corridor corridorId=%d "
             "progress=%.2f playerDistance=%.2f position=(%.2f, %.2f, %.2f), "
             "active=false\n",
             salaryman.corridorId, corridor_progress, player_distance,
             salaryman.position.x, salaryman.position.y, salaryman.position.z);
    }
    salaryman.active = false;
  } else if (player_distance > 55.0f) {
    if (kCorridorDebugLogsEnabled) {
      printf("Salaryman despawn: reason=too_far corridorId=%d progress=%.2f "
             "playerDistance=%.2f position=(%.2f, %.2f, %.2f), active=false\n",
             salaryman.corridorId, corridor_progress, player_distance,
             salaryman.position.x, salaryman.position.y, salaryman.position.z);
    }
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
  const glm::mat4 model_matrix =
      Matrix(right.x, corrected_up.x, forward.x, p.x, right.y, corrected_up.y,
             forward.y, p.y, right.z, corrected_up.z, forward.z, p.z, 0.0f,
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
