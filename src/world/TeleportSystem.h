#ifndef WORLD_TELEPORTSYSTEM_H
#define WORLD_TELEPORTSYSTEM_H

#include "collisions.h"
#include <glm/vec4.hpp>

void UpdateTeleportSystem(const CollisionResult &collision, glm::vec4 &camera_position);

#endif
