#include "entities/Player.h"

#include "engine/Renderer.h"
#include "engine/Shader.h"
#include "matrices.h"

#include <cmath>
#include <cstdio>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

SalarymanAnimatedModel g_PlayerAnimatedModel;
SalarymanAnimator g_PlayerAnimator;
PlayerCharacter g_PlayerCharacter;

namespace {
constexpr float kPlayerWorldSpeedMultiplier = 2.75f;
constexpr float kPlayerAnimationPlaybackScale = 1.25f;
constexpr float kPlayerRequestedSpeedScale = 1.5f;
}

PlayerCharacter::PlayerCharacter()
    : loaded(false), moving(false), useAnimation(false),
      position(0.0f, 0.0f, -1.0f), forward(0.0f, 0.0f, -1.0f), yaw(0.0f),
      speed(1.5f), baseWalkSpeed(0.67f), animationPlaybackScale(1.0f),
      locomotionScale(1.0f), model(NULL), animatedModel(NULL), animator(NULL) {}

bool LoadPlayerCharacterModel() {
  g_PlayerCharacter.model = NULL;
  g_PlayerCharacter.animatedModel = &g_PlayerAnimatedModel;
  g_PlayerCharacter.animator = &g_PlayerAnimator;

  const char *model_path = "assets/characterwalking.fbx";
  if (LoadTexturedAnimatedModel(g_PlayerAnimatedModel, model_path,
                                "assets/character/Ch09_1001_Diffuse.png",
                                NULL, "player character")) {
    g_PlayerAnimator.model = &g_PlayerAnimatedModel;
    g_PlayerAnimator.currentTime = 0.0f;
    UpdateSalarymanAnimation(g_PlayerAnimator, 0.0f);
    g_PlayerCharacter.loaded = true;
    g_PlayerCharacter.useAnimation = true;
    if (g_PlayerAnimatedModel.recommendedWalkSpeed > 0.05f)
      g_PlayerCharacter.baseWalkSpeed = g_PlayerAnimatedModel.recommendedWalkSpeed;
    else
      g_PlayerCharacter.baseWalkSpeed = 0.70f;
    g_PlayerCharacter.animationPlaybackScale = kPlayerAnimationPlaybackScale;
    g_PlayerCharacter.speed =
        g_PlayerCharacter.baseWalkSpeed * kPlayerWorldSpeedMultiplier *
        kPlayerRequestedSpeedScale;
    printf("Player character movement retimed: baseSpeed=%.3f "
           "worldMultiplier=%.2f requestedSpeedScale=%.2f "
           "animationPlayback=%.2f finalSpeed=%.3f cycleDistanceScale=%.2f\n",
           g_PlayerCharacter.baseWalkSpeed, kPlayerWorldSpeedMultiplier,
           kPlayerRequestedSpeedScale,
           g_PlayerCharacter.animationPlaybackScale, g_PlayerCharacter.speed,
           (kPlayerWorldSpeedMultiplier * kPlayerRequestedSpeedScale) /
               kPlayerAnimationPlaybackScale);
    printf("Player character render mode: animated\n");
    return true;
  }

  g_PlayerCharacter.loaded = false;
  g_PlayerCharacter.useAnimation = false;
  fprintf(stderr,
          "ERROR: Player character animated model could not be loaded.\n");
  return false;
}

void InitializePlayerCharacterFromCamera(const glm::vec4 &camera_position,
                                         float camera_yaw) {
  g_PlayerCharacter.position =
      glm::vec3(camera_position.x, 0.0f, camera_position.z);
  g_PlayerCharacter.yaw = camera_yaw;

  glm::vec3 forward(std::sin(camera_yaw), 0.0f, -std::cos(camera_yaw));
  if (glm::length(forward) < 0.0001f)
    forward = glm::vec3(0.0f, 0.0f, -1.0f);
  else
    forward = glm::normalize(forward);
  g_PlayerCharacter.forward = forward;
}

float GetPlayerThirdPersonShiftSprintSpeed() {
  return g_PlayerCharacter.speed * kThirdPersonShiftSprintMultiplier;
}

void UpdatePlayerCharacterAnimation(PlayerCharacter &player,
                                    float delta_time) {
  if (!player.loaded || !player.useAnimation || player.animator == NULL)
    return;

  if (player.moving) {
    UpdateSalarymanAnimation(*player.animator,
                             delta_time * player.animationPlaybackScale *
                                 player.locomotionScale);
  } else {
    player.animator->currentTime = 0.0f;
    UpdateSalarymanAnimation(*player.animator, 0.0f);
  }
}

void DrawPlayerCharacter(const PlayerCharacter &player,
                         const Material &material) {
  if (!player.loaded)
    return;

  glm::vec3 forward = player.forward;
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
  const glm::vec3 p = player.position;
  const glm::mat4 model_matrix =
      Matrix(right.x, corrected_up.x, forward.x, p.x, right.y, corrected_up.y,
             forward.y, p.y, right.z, corrected_up.z, forward.z, p.z, 0.0f,
             0.0f, 0.0f, 1.0f);

  ApplyMaterial(material);

  if (player.useAnimation && player.animatedModel != NULL &&
      player.animatedModel->loaded) {
    const glm::mat4 animated_model_matrix =
        model_matrix * player.animatedModel->normalizationMatrix;
    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                       glm::value_ptr(animated_model_matrix));
    DrawAnimatedModel(*player.animatedModel);
  } else if (player.model != NULL) {
    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                       glm::value_ptr(model_matrix));
    DrawStaticModel(*player.model);
  }
}
