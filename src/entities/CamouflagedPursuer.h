#ifndef ENTITIES_CAMOUFLAGEDPURSUER_H
#define ENTITIES_CAMOUFLAGEDPURSUER_H

#include "entities/AnimatedModel.h"
#include "rendering/Material.h"
#include "world/Corridor.h"

#include <glm/vec3.hpp>

struct CamouflagedPursuerState {
  bool active;
  bool visible;
  bool chasing;
  int corridorId;
  float triggerRadius;
  float movementSpeed;
  glm::vec3 position;
  glm::vec3 forward;
  bool useAnimation;
  StaticModel *placeholderModel;
  SalarymanAnimatedModel *animatedModel;
  SalarymanAnimator *animator;

  CamouflagedPursuerState();
};

extern CamouflagedPursuerState g_CamouflagedPursuer;
extern SalarymanAnimatedModel g_CamouflagedPursuerAnimatedModel;
extern SalarymanAnimator g_CamouflagedPursuerAnimator;

void ResetCamouflagedPursuer(CamouflagedPursuerState &pursuer);
void ActivateCamouflagedPursuerForCorridor(
    CamouflagedPursuerState &pursuer, const CorridorContent &content);
void UpdateCamouflagedPursuer(CamouflagedPursuerState &pursuer,
                              float delta_time,
                              const glm::vec3 &player_position);
bool HasCamouflagedPursuerCaughtPlayer(
    const CamouflagedPursuerState &pursuer,
    const glm::vec3 &player_position);
void DrawCamouflagedPursuer(const CamouflagedPursuerState &pursuer,
                            const Material &material);

#endif
