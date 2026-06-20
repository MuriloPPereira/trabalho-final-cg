#ifndef ENTITIES_NPC_H
#define ENTITIES_NPC_H

#include "entities/AnimatedModel.h"
#include "rendering/Material.h"
#include "world/Corridor.h"
#include <glm/vec4.hpp>

struct SalarymanNPC {
  bool active;
  int corridorId;
  glm::vec3 position;
  glm::vec3 forward;
  float speed;
  float corridorLength;
  glm::vec3 corridorOrigin;
  bool useAnimation;
  bool useBezier;
  bool reverseBezier;
  float bezierT;
  float pathStartProgress;
  float pathTravelDistance;
  float spawnGraceTimer; // seconds remaining before distance-despawn activates
  glm::vec3 p0, p1, p2, p3;
  StaticModel *model;
  SalarymanAnimatedModel *animatedModel;
  SalarymanAnimator *animator;
  bool isGiant;
  SalarymanNPC();
};

extern StaticModel g_SalarymanStaticModel;
extern SalarymanAnimatedModel g_SalarymanAnimatedModel;
extern SalarymanAnimator g_SalarymanAnimator;
extern SalarymanNPC g_SalarymanNPC;

void SpawnSalarymanForCorridor(SalarymanNPC &salaryman, const CorridorContent &content, const glm::vec3 &player_position);
void UpdateSalarymanNPC(SalarymanNPC &salaryman, float delta_time, const glm::vec4 &camera_position_c);
void DrawSalarymanNPC(const SalarymanNPC &salaryman, const Material &material);
void TrySpawnSalarymanForCorridorContent(const CorridorContent &content, const glm::vec3 &player_position, const char *reason);

#endif
