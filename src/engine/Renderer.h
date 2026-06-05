#ifndef ENGINE_RENDERER_H
#define ENGINE_RENDERER_H

#include "rendering/Material.h"
#include <glm/mat4x4.hpp>
#include <vector>

void PushMatrix(glm::mat4 M);
void PopMatrix(glm::mat4 &M);
void ApplyMaterial(const Material &material);
void SetPointLights(const std::vector<PointLight> &lights);
void DrawVirtualObject(const char *object_name);

#endif
