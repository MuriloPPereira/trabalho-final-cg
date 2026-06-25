#ifndef RENDERING_MATERIAL_H
#define RENDERING_MATERIAL_H

#include <glad/glad.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct Material {
  // Index into the loaded texture registry. Rendering binds it to a safe
  // hardware unit before updating the sampler uniform.
  GLuint diffuse_texture_unit;
  float specular_strength;
  float shininess;
  float ambient_strength;
  glm::vec2 uv_scale;
  glm::vec2 uv_offset;
};

struct PointLight {
  glm::vec3 position;
  glm::vec3 color;
  float ambient_strength;
  float diffuse_strength;
  float specular_strength;
  float constant;
  float linear;
  float quadratic;
};

#endif
