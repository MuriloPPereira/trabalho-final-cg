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
      forward(0.0f, 0.0f, -1.0f), speed(2.5f), corridorLength(0.0f),
      corridorOrigin(0.0f, 0.0f, 0.0f), useAnimation(false), useBezier(false),
      reverseBezier(false), bezierT(0.0f), spawnGraceTimer(0.0f), p0(0.0f), p1(0.0f), p2(0.0f),
      p3(0.0f), model(NULL), animatedModel(NULL), animator(NULL), isGiant(false) {}

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

  // Bezier setup
  salaryman.useBezier = true;
  salaryman.reverseBezier = (path_forward.z < 0.0f);
  salaryman.bezierT = 0.0f;
  // Segment points will be evaluated dynamically in UpdateSalarymanNPC
  if (salaryman.reverseBezier) {
    salaryman.position = glm::vec3(6.0f, 0.0f, kCorridorZ0 + 2.0f);
    salaryman.forward = glm::vec3(-1.0f, 0.0f, 0.0f);
  } else {
    // Just set initial position to the far turn connector
    salaryman.position = glm::vec3(-6.0f, 0.0f, kCorridorZ1 - 2.0f);
    salaryman.forward = glm::vec3(1.0f, 0.0f, 0.0f);
  }
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
    // Total path length is roughly 52 units
    float curve_length_approx = 52.0f;
    salaryman.bezierT += (salaryman.speed / curve_length_approx) * delta_time;

    if (salaryman.bezierT >= 1.0f) {
      salaryman.active = false;
      return;
    }

    glm::vec3 new_pos;
    float global_t = salaryman.reverseBezier ? (1.0f - salaryman.bezierT) : salaryman.bezierT;

    auto eval_bezier = [](float t, glm::vec3 p0, glm::vec3 p1, glm::vec3 p2,
                          glm::vec3 p3) {
      float u = 1.0f - t;
      return (u * u * u) * p0 + 3.0f * (u * u) * t * p1 +
             3.0f * u * (t * t) * p2 + (t * t * t) * p3;
    };

    if (global_t < 0.2f) {
      // Segment 1: Far turn (from -X connector to straight)
      float t = global_t / 0.2f;
      new_pos = eval_bezier(t, glm::vec3(-6.0f, 0.0f, kCorridorZ1 - 2.0f),
                            glm::vec3(0.0f, 0.0f, kCorridorZ1 - 2.0f),
                            glm::vec3(0.0f, 0.0f, kCorridorZ1 + 2.0f),
                            glm::vec3(0.0f, 0.0f, kCorridorZ1 + 4.0f));
    } else if (global_t < 0.8f) {
      // Segment 2: Straight corridor
      float t = (global_t - 0.2f) / 0.6f;
      glm::vec3 start = glm::vec3(0.0f, 0.0f, kCorridorZ1 + 4.0f);
      glm::vec3 end = glm::vec3(0.0f, 0.0f, kCorridorZ0 - 4.0f);
      new_pos = start + t * (end - start);
    } else {
      // Segment 3: Near turn (from straight to +X connector)
      float t = (global_t - 0.8f) / 0.2f;
      new_pos = eval_bezier(t, glm::vec3(0.0f, 0.0f, kCorridorZ0 - 4.0f),
                            glm::vec3(0.0f, 0.0f, kCorridorZ0 - 2.0f),
                            glm::vec3(0.0f, 0.0f, kCorridorZ0 + 2.0f),
                            glm::vec3(6.0f, 0.0f, kCorridorZ0 + 2.0f));
    }

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
