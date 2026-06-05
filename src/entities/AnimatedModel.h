#ifndef ENTITIES_ANIMATEDMODEL_H
#define ENTITIES_ANIMATEDMODEL_H

#include "utils/Constants.h"
#include <glad/glad.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

struct StaticModel { std::vector<std::string> object_names; glm::vec3 bbox_min; glm::vec3 bbox_max; };
struct BoneInfo { int id; glm::mat4 offsetMatrix; glm::mat4 finalTransform; BoneInfo(); };
struct SalarymanAnimatedMesh {
  std::string name;
  std::string materialName;
  GLuint vertex_array_object_id;
  GLuint vertex_buffer_id;
  GLuint index_buffer_id;
  GLuint diffuse_texture_id;
  GLuint diffuse_texture_unit;
  unsigned int material_index;
  size_t num_indices;
  glm::vec3 bbox_min;
  glm::vec3 bbox_max;
  SalarymanAnimatedMesh();
};
struct SalarymanPositionKey { float time; glm::vec3 value; };
struct SalarymanRotationKey { float time; glm::quat value; };
struct SalarymanScaleKey { float time; glm::vec3 value; };
struct SalarymanAnimationChannel { std::string nodeName; std::vector<SalarymanPositionKey> positions; std::vector<SalarymanRotationKey> rotations; std::vector<SalarymanScaleKey> scales; };
struct SalarymanAnimation { float duration; float ticksPerSecond; std::vector<SalarymanAnimationChannel> channels; std::map<std::string, int> channelByNodeName; SalarymanAnimation(); };
struct SalarymanAnimatedNode { std::string name; glm::mat4 transform; std::vector<int> children; SalarymanAnimatedNode(); };
struct SalarymanAnimatedModel {
  bool loaded;
  std::vector<SalarymanAnimatedMesh> meshes;
  std::map<std::string, BoneInfo> boneInfoMap;
  int boneCount;
  int meshCount;
  int animationCount;
  glm::mat4 globalInverseTransform;
  glm::mat4 normalizationMatrix;
  SalarymanAnimation animation;
  std::vector<SalarymanAnimatedNode> nodes;
  int rootNodeIndex;
  std::vector<glm::mat4> finalBoneMatrices;
  float rootMotionDistance;
  float animationDurationSeconds;
  float recommendedWalkSpeed;
  SalarymanAnimatedModel();
};
struct SalarymanAnimator { SalarymanAnimatedModel *model; float currentTime; SalarymanAnimator(); };

bool LoadSalarymanStaticModel(StaticModel &model, const char *filename);
bool LoadTexturedAnimatedModel(SalarymanAnimatedModel &model,
                               const char *filename,
                               const char *body_diffuse_filename,
                               const char *hair_diffuse_filename,
                               const char *debug_label);
bool LoadSalarymanAnimatedModel(SalarymanAnimatedModel &model, const char *filename);
void UpdateSalarymanAnimation(SalarymanAnimator &animator, float delta_time);
void DrawAnimatedModel(const SalarymanAnimatedModel &model);
void DrawStaticModel(const StaticModel &model);

#endif
