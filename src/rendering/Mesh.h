#ifndef RENDERING_MESH_H
#define RENDERING_MESH_H

#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <cstddef>
#include <map>
#include <string>
#include <vector>
#include <tiny_obj_loader.h>

struct ObjModel {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  ObjModel(const char *filename, const char *basepath = NULL, bool triangulate = true);
};

struct SceneObject {
  std::string name;
  size_t first_index;
  size_t num_indices;
  GLenum rendering_mode;
  GLuint vertex_array_object_id;
  glm::vec3 bbox_min;
  glm::vec3 bbox_max;
};

extern std::map<std::string, SceneObject> g_VirtualScene;

void ComputeNormals(ObjModel *model);
void BuildTrianglesAndAddToVirtualScene(ObjModel *model);
void PrintObjModelInfo(ObjModel *model);

bool LoadModelWithAssimpToVirtualScene(const char* filename, const char* object_name);

#endif
