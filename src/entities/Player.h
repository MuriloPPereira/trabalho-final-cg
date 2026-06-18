#ifndef ENTITIES_PLAYER_H
#define ENTITIES_PLAYER_H

#include "entities/AnimatedModel.h"
#include "rendering/Material.h"
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

struct PlayerCharacter {
  bool loaded;
  bool moving;
  bool useAnimation;
  glm::vec3 position;
  glm::vec3 forward;
  float yaw;
  float speed;
  float baseWalkSpeed;
  float animationPlaybackScale;
  float locomotionScale;
  StaticModel *model;
  SalarymanAnimatedModel *animatedModel;
  SalarymanAnimator *animator;
  PlayerCharacter();
};

extern SalarymanAnimatedModel g_PlayerAnimatedModel;
extern SalarymanAnimator g_PlayerAnimator;
extern PlayerCharacter g_PlayerCharacter;

bool LoadPlayerCharacterModel();
void InitializePlayerCharacterFromCamera(const glm::vec4 &camera_position,
                                         float camera_yaw);
float GetPlayerThirdPersonShiftSprintSpeed();
void UpdatePlayerCharacterAnimation(PlayerCharacter &player, float delta_time);
void DrawPlayerCharacter(const PlayerCharacter &player,
                         const Material &material);

#endif
