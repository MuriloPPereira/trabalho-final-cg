#include "engine/Renderer.h"

#include "engine/Shader.h"
#include "engine/Texture.h"
#include "matrices.h"
#include "rendering/Mesh.h"

#include <algorithm>
#include <cstdio>
#include <stack>
#include <glm/gtc/type_ptr.hpp>

std::stack<glm::mat4> g_MatrixStack;

void ApplyMaterial(const Material &material) {
  constexpr GLuint kMaterialTextureUnit = 0;
  BindLoadedTexture(material.diffuse_texture_unit, kMaterialTextureUnit);
  glUniform1i(g_material_diffuse_uniform, kMaterialTextureUnit);
  glUniform1f(g_material_specular_strength_uniform, material.specular_strength);
  glUniform1f(g_material_shininess_uniform, material.shininess);
  glUniform1f(g_material_ambient_strength_uniform, material.ambient_strength);
  glUniform2f(g_material_uv_scale_uniform, material.uv_scale.x,
              material.uv_scale.y);
  glUniform2f(g_material_uv_offset_uniform, material.uv_offset.x,
              material.uv_offset.y);
}

void SetPointLights(const std::vector<PointLight> &lights) {
  int count = static_cast<int>(
      std::min(lights.size(), static_cast<size_t>(kMaxLights)));
  glUniform1i(g_num_lights_uniform, count);

  for (int i = 0; i < count; ++i) {
    const PointLight &light = lights[i];
    glUniform3f(g_light_position_uniforms[i], light.position.x,
                light.position.y, light.position.z);
    glUniform3f(g_light_color_uniforms[i], light.color.x, light.color.y,
                light.color.z);
    glUniform1f(g_light_ambient_strength_uniforms[i], light.ambient_strength);
    glUniform1f(g_light_diffuse_strength_uniforms[i], light.diffuse_strength);
    glUniform1f(g_light_specular_strength_uniforms[i], light.specular_strength);
    glUniform1f(g_light_constant_uniforms[i], light.constant);
    glUniform1f(g_light_linear_uniforms[i], light.linear);
    glUniform1f(g_light_quadratic_uniforms[i], light.quadratic);
  }
}

// Função que desenha um objeto armazenado em g_VirtualScene. Veja definição
// dos objetos na função BuildTrianglesAndAddToVirtualScene().
void DrawVirtualObject(const char *object_name) {
  if (g_use_skinning_uniform >= 0)
    glUniform1i(g_use_skinning_uniform, GL_FALSE);

  const auto object_it = g_VirtualScene.find(object_name);
  if (object_it == g_VirtualScene.end()) {
    fprintf(stderr, "ERROR: Virtual scene object \"%s\" was not found.\n",
            object_name);
    return;
  }
  const SceneObject &object = object_it->second;

  // "Ligamos" o VAO. Informamos que queremos utilizar os atributos de
  // vértices apontados pelo VAO criado pela função
  // BuildTrianglesAndAddToVirtualScene(). Veja comentários detalhados dentro
  // da definição de BuildTrianglesAndAddToVirtualScene().
  glBindVertexArray(object.vertex_array_object_id);

  // Setamos as variáveis "bbox_min" e "bbox_max" do fragment shader
  // com os parâmetros da axis-aligned bounding box (AABB) do modelo.
  glm::vec3 bbox_min = object.bbox_min;
  glm::vec3 bbox_max = object.bbox_max;
  glUniform4f(g_bbox_min_uniform, bbox_min.x, bbox_min.y, bbox_min.z, 1.0f);
  glUniform4f(g_bbox_max_uniform, bbox_max.x, bbox_max.y, bbox_max.z, 1.0f);

  // Pedimos para a GPU rasterizar os vértices dos eixos XYZ
  // apontados pelo VAO como linhas. Veja a definição de
  // g_VirtualScene[""] dentro da função BuildTrianglesAndAddToVirtualScene(),
  // e veja a documentação da função glDrawElements() em
  // http://docs.gl/gl3/glDrawElements.
  glDrawElements(
      object.rendering_mode, object.num_indices, GL_UNSIGNED_INT,
      (void *)(object.first_index * sizeof(GLuint)));

  // "Desligamos" o VAO, evitando assim que operações posteriores venham a
  // alterar o mesmo. Isso evita bugs.
  glBindVertexArray(0);
}

// Função que pega a matriz M e guarda a mesma no topo da pilha
void PushMatrix(glm::mat4 M) { g_MatrixStack.push(M); }

// Função que remove a matriz atualmente no topo da pilha e armazena a mesma
// na variável M
void PopMatrix(glm::mat4 &M) {
  if (g_MatrixStack.empty()) {
    M = Matrix_Identity();
  } else {
    M = g_MatrixStack.top();
    g_MatrixStack.pop();
  }
}
