#include "entities/AnimatedModel.h"

#include "engine/Renderer.h"
#include "engine/Shader.h"
#include "engine/Texture.h"
#include "matrices.h"
#include "rendering/Mesh.h"
#include "utils/FileUtils.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stb_image.h>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

StaticModel::StaticModel()
    : loaded(false), object_names(), bbox_min(0.0f, 0.0f, 0.0f),
      bbox_max(0.0f, 0.0f, 0.0f) {}
BoneInfo::BoneInfo() : id(-1), offsetMatrix(Matrix_Identity()), finalTransform(Matrix_Identity()) {}
SalarymanAnimatedMesh::SalarymanAnimatedMesh()
    : name(), materialName(), vertex_array_object_id(0), vertex_buffer_id(0),
      index_buffer_id(0), diffuse_texture_id(0), diffuse_texture_unit(0),
      material_index(0), num_indices(0), bbox_min(0.0f, 0.0f, 0.0f),
      bbox_max(0.0f, 0.0f, 0.0f) {}
SalarymanAnimation::SalarymanAnimation() : duration(0.0f), ticksPerSecond(25.0f) {}
SalarymanAnimatedNode::SalarymanAnimatedNode() : transform(Matrix_Identity()) {}
SalarymanAnimatedModel::SalarymanAnimatedModel()
    : loaded(false), boneCount(0), meshCount(0), animationCount(0),
      globalInverseTransform(Matrix_Identity()), normalizationMatrix(Matrix_Identity()),
      rootNodeIndex(-1), rootMotionDistance(0.0f),
      animationDurationSeconds(0.0f), recommendedWalkSpeed(0.0f) {}
SalarymanAnimator::SalarymanAnimator() : model(NULL), currentTime(0.0f) {}

namespace {
struct FbxNodeHeader {
  uint64_t end_offset;
  uint64_t num_properties;
  uint64_t property_list_length;
  std::string name;
};

struct SalarymanStaticVertex {
  float px, py, pz, pw;
  float nx, ny, nz, nw;
  float u, v;
};

struct FbxMeshData {
  std::vector<double> vertices;
  std::vector<int32_t> polygon_indices;
  std::vector<double> normals;
  std::vector<double> uvs;
  std::vector<int32_t> uv_indices;
  std::string normal_mapping;
  std::string normal_reference;
  std::string uv_mapping;
  std::string uv_reference;
};

uint32_t ReadU32(const std::vector<unsigned char> &bytes, size_t offset) {
  uint32_t value;
  std::memcpy(&value, &bytes[offset], sizeof(value));
  return value;
}

uint64_t ReadU64(const std::vector<unsigned char> &bytes, size_t offset) {
  uint64_t value;
  std::memcpy(&value, &bytes[offset], sizeof(value));
  return value;
}

int64_t ReadI64(const std::vector<unsigned char> &bytes, size_t offset) {
  int64_t value;
  std::memcpy(&value, &bytes[offset], sizeof(value));
  return value;
}

size_t FbxHeaderSize(uint32_t version) { return (version >= 7500) ? 25 : 13; }

size_t FbxNullRecordSize(uint32_t version) { return FbxHeaderSize(version); }

bool ReadFbxNodeHeader(const std::vector<unsigned char> &bytes,
                       uint32_t version, size_t &offset,
                       FbxNodeHeader &header) {
  if (offset + FbxHeaderSize(version) > bytes.size())
    return false;

  uint64_t name_length = 0;
  if (version >= 7500) {
    header.end_offset = ReadU64(bytes, offset + 0);
    header.num_properties = ReadU64(bytes, offset + 8);
    header.property_list_length = ReadU64(bytes, offset + 16);
    name_length = bytes[offset + 24];
    offset += 25;
  } else {
    header.end_offset = ReadU32(bytes, offset + 0);
    header.num_properties = ReadU32(bytes, offset + 4);
    header.property_list_length = ReadU32(bytes, offset + 8);
    name_length = bytes[offset + 12];
    offset += 13;
  }

  if (header.end_offset == 0 && header.num_properties == 0 &&
      header.property_list_length == 0 && name_length == 0)
    return false;

  if (offset + name_length > bytes.size())
    return false;

  header.name.assign((const char *)&bytes[offset], (size_t)name_length);
  offset += (size_t)name_length;
  return true;
}

bool SkipFbxProperty(const std::vector<unsigned char> &bytes, size_t &offset) {
  if (offset >= bytes.size())
    return false;

  const char type = (char)bytes[offset++];
  switch (type) {
  case 'Y':
    offset += 2;
    return offset <= bytes.size();
  case 'C':
    offset += 1;
    return offset <= bytes.size();
  case 'I':
    offset += 4;
    return offset <= bytes.size();
  case 'F':
    offset += 4;
    return offset <= bytes.size();
  case 'D':
    offset += 8;
    return offset <= bytes.size();
  case 'L':
    offset += 8;
    return offset <= bytes.size();
  case 'R':
  case 'S': {
    if (offset + 4 > bytes.size())
      return false;
    const uint32_t length = ReadU32(bytes, offset);
    offset += 4 + length;
    return offset <= bytes.size();
  }
  case 'f':
  case 'd':
  case 'i':
  case 'l':
  case 'b': {
    if (offset + 12 > bytes.size())
      return false;
    const uint32_t compressed_length = ReadU32(bytes, offset + 8);
    offset += 12 + compressed_length;
    return offset <= bytes.size();
  }
  default:
    return false;
  }
}

bool SkipFbxProperties(const std::vector<unsigned char> &bytes, uint64_t count,
                       size_t &offset) {
  for (uint64_t i = 0; i < count; ++i) {
    if (!SkipFbxProperty(bytes, offset))
      return false;
  }
  return true;
}

bool ReadFbxStringProperty(const std::vector<unsigned char> &bytes,
                           size_t &offset, std::string &value) {
  if (offset + 5 > bytes.size() || bytes[offset] != 'S')
    return false;

  ++offset;
  const uint32_t length = ReadU32(bytes, offset);
  offset += 4;
  if (offset + length > bytes.size())
    return false;

  value.assign((const char *)&bytes[offset], (size_t)length);
  offset += length;
  return true;
}

template <typename T>
bool ReadFbxArrayProperty(const std::vector<unsigned char> &bytes,
                          size_t &offset, char expected_type,
                          std::vector<T> &values) {
  if (offset + 13 > bytes.size() || bytes[offset] != expected_type)
    return false;

  ++offset;
  const uint32_t count = ReadU32(bytes, offset + 0);
  const uint32_t encoding = ReadU32(bytes, offset + 4);
  const uint32_t compressed_length = ReadU32(bytes, offset + 8);
  offset += 12;

  if (offset + compressed_length > bytes.size())
    return false;

  const size_t expected_size = (size_t)count * sizeof(T);
  values.resize(count);

  if (encoding == 0) {
    if (compressed_length < expected_size)
      return false;
    std::memcpy(values.data(), &bytes[offset], expected_size);
  } else if (encoding == 1) {
    int decoded_length = 0;
    char *decoded = stbi_zlib_decode_malloc_guesssize(
        (const char *)&bytes[offset], (int)compressed_length,
        (int)expected_size, &decoded_length);
    if (decoded == NULL || decoded_length < (int)expected_size) {
      if (decoded != NULL)
        stbi_image_free(decoded);
      return false;
    }

    std::memcpy(values.data(), decoded, expected_size);
    stbi_image_free(decoded);
  } else {
    return false;
  }

  offset += compressed_length;
  return true;
}


bool LoadAnimatedDiffuseTexture(const char *debug_label, const char *filename,
                                GLuint &texture_id, GLuint &texture_unit) {
  const std::string fullpath = ResolveExistingPath(filename);
  printf("Loading %s diffuse texture \"%s\"... ", debug_label,
         fullpath.c_str());

  stbi_set_flip_vertically_on_load(true);
  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char *data =
      stbi_load(fullpath.c_str(), &width, &height, &channels, 3);
  if (data == NULL) {
    fprintf(stderr,
            "FAILED.\nERROR: Cannot open %s diffuse texture \"%s\".\n",
            debug_label, fullpath.c_str());
    texture_id = 0;
    texture_unit = 0;
    return false;
  }

  texture_id = 0;
  GLuint sampler_id = 0;
  glGenTextures(1, &texture_id);
  glGenSamplers(1, &sampler_id);

  glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER,
                      GL_LINEAR_MIPMAP_LINEAR);
  glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  texture_unit = g_NumLoadedTextures;
  glActiveTexture(GL_TEXTURE0 + texture_unit);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB,
               GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);
  glBindSampler(texture_unit, sampler_id);
  glBindTexture(GL_TEXTURE_2D, 0);

  stbi_image_free(data);
  g_NumLoadedTextures += 1;

  printf("OK (%dx%d, textureId=%u, unit=%u).\n", width, height, texture_id,
         texture_unit);
  return true;
}

bool ParseFbxLayerElementNormal(const std::vector<unsigned char> &bytes,
                                uint32_t version, size_t start, uint64_t end,
                                FbxMeshData &mesh) {
  size_t offset = start;
  while (offset < (size_t)end - FbxNullRecordSize(version)) {
    FbxNodeHeader child;
    if (!ReadFbxNodeHeader(bytes, version, offset, child))
      break;

    size_t prop_offset = offset;
    if (child.name == "Normals")
      ReadFbxArrayProperty<double>(bytes, prop_offset, 'd', mesh.normals);
    else if (child.name == "MappingInformationType")
      ReadFbxStringProperty(bytes, prop_offset, mesh.normal_mapping);
    else if (child.name == "ReferenceInformationType")
      ReadFbxStringProperty(bytes, prop_offset, mesh.normal_reference);

    offset = (size_t)child.end_offset;
  }

  return !mesh.normals.empty();
}

bool ParseFbxLayerElementUV(const std::vector<unsigned char> &bytes,
                            uint32_t version, size_t start, uint64_t end,
                            FbxMeshData &mesh) {
  size_t offset = start;
  while (offset < (size_t)end - FbxNullRecordSize(version)) {
    FbxNodeHeader child;
    if (!ReadFbxNodeHeader(bytes, version, offset, child))
      break;

    size_t prop_offset = offset;
    if (child.name == "UV")
      ReadFbxArrayProperty<double>(bytes, prop_offset, 'd', mesh.uvs);
    else if (child.name == "UVIndex")
      ReadFbxArrayProperty<int32_t>(bytes, prop_offset, 'i', mesh.uv_indices);
    else if (child.name == "MappingInformationType")
      ReadFbxStringProperty(bytes, prop_offset, mesh.uv_mapping);
    else if (child.name == "ReferenceInformationType")
      ReadFbxStringProperty(bytes, prop_offset, mesh.uv_reference);

    offset = (size_t)child.end_offset;
  }

  return !mesh.uvs.empty();
}

bool ParseFbxGeometry(const std::vector<unsigned char> &bytes, uint32_t version,
                      size_t start, uint64_t end, FbxMeshData &mesh) {
  size_t offset = start;
  while (offset < (size_t)end - FbxNullRecordSize(version)) {
    FbxNodeHeader child;
    if (!ReadFbxNodeHeader(bytes, version, offset, child))
      break;

    size_t prop_offset = offset;
    if (child.name == "Vertices")
      ReadFbxArrayProperty<double>(bytes, prop_offset, 'd', mesh.vertices);
    else if (child.name == "PolygonVertexIndex")
      ReadFbxArrayProperty<int32_t>(bytes, prop_offset, 'i',
                                    mesh.polygon_indices);
    else if (child.name == "LayerElementNormal") {
      SkipFbxProperties(bytes, child.num_properties, prop_offset);
      ParseFbxLayerElementNormal(bytes, version, prop_offset, child.end_offset,
                                 mesh);
    } else if (child.name == "LayerElementUV") {
      SkipFbxProperties(bytes, child.num_properties, prop_offset);
      ParseFbxLayerElementUV(bytes, version, prop_offset, child.end_offset,
                             mesh);
    }

    offset = (size_t)child.end_offset;
  }

  return !mesh.vertices.empty() && !mesh.polygon_indices.empty();
}

bool AppendFbxMeshAsStaticVertices(
    const FbxMeshData &mesh, std::vector<SalarymanStaticVertex> &vertices) {
  std::vector<int> polygon_vertices;
  std::vector<int> polygon_vertex_ordinals;
  size_t polygon_start_ordinal = 0;

  auto emit_vertex = [&](int polygon_vertex_index) {
    const int vertex_index = polygon_vertices[polygon_vertex_index];
    const int polygon_ordinal = polygon_vertex_ordinals[polygon_vertex_index];
    if (vertex_index < 0 ||
        (size_t)(3 * vertex_index + 2) >= mesh.vertices.size())
      return;

    SalarymanStaticVertex vertex;
    vertex.px = (float)mesh.vertices[3 * vertex_index + 0];
    vertex.py = (float)mesh.vertices[3 * vertex_index + 1];
    vertex.pz = (float)mesh.vertices[3 * vertex_index + 2];
    vertex.pw = 1.0f;

    glm::vec3 normal(0.0f, 1.0f, 0.0f);
    if (mesh.normal_mapping == "ByPolygonVertex" &&
        mesh.normal_reference == "Direct" &&
        (size_t)(3 * polygon_ordinal + 2) < mesh.normals.size()) {
      normal = glm::vec3((float)mesh.normals[3 * polygon_ordinal + 0],
                         (float)mesh.normals[3 * polygon_ordinal + 1],
                         (float)mesh.normals[3 * polygon_ordinal + 2]);
    } else if (mesh.normal_mapping == "ByVertice" &&
               (size_t)(3 * vertex_index + 2) < mesh.normals.size()) {
      normal = glm::vec3((float)mesh.normals[3 * vertex_index + 0],
                         (float)mesh.normals[3 * vertex_index + 1],
                         (float)mesh.normals[3 * vertex_index + 2]);
    }

    if (glm::length(normal) > 0.0001f)
      normal = glm::normalize(normal);
    vertex.nx = normal.x;
    vertex.ny = normal.y;
    vertex.nz = normal.z;
    vertex.nw = 0.0f;

    vertex.u = 0.0f;
    vertex.v = 0.0f;
    if (mesh.uv_mapping == "ByPolygonVertex") {
      int uv_index = polygon_ordinal;
      if (mesh.uv_reference == "IndexToDirect" &&
          (size_t)polygon_ordinal < mesh.uv_indices.size())
        uv_index = mesh.uv_indices[polygon_ordinal];

      if (uv_index >= 0 && (size_t)(2 * uv_index + 1) < mesh.uvs.size()) {
        vertex.u = (float)mesh.uvs[2 * uv_index + 0];
        vertex.v = (float)mesh.uvs[2 * uv_index + 1];
      }
    }

    vertices.push_back(vertex);
  };

  for (size_t i = 0; i < mesh.polygon_indices.size(); ++i) {
    int index = mesh.polygon_indices[i];
    const bool last_vertex_in_polygon = index < 0;
    if (last_vertex_in_polygon)
      index = -index - 1;

    polygon_vertices.push_back(index);
    polygon_vertex_ordinals.push_back(
        (int)(polygon_start_ordinal + polygon_vertices.size() - 1));

    if (last_vertex_in_polygon) {
      if (polygon_vertices.size() >= 3) {
        for (size_t triangle = 1; triangle + 1 < polygon_vertices.size();
             ++triangle) {
          emit_vertex(0);
          emit_vertex((int)triangle);
          emit_vertex((int)triangle + 1);
        }
      }

      polygon_start_ordinal = i + 1;
      polygon_vertices.clear();
      polygon_vertex_ordinals.clear();
    }
  }

  return true;
}

struct SalarymanAnimatedVertex {
  float px, py, pz, pw;
  float nx, ny, nz, nw;
  float u, v;
  int bone_ids[4];
  float bone_weights[4];

  SalarymanAnimatedVertex()
      : px(0.0f), py(0.0f), pz(0.0f), pw(1.0f), nx(0.0f), ny(1.0f), nz(0.0f),
        nw(0.0f), u(0.0f), v(0.0f) {
    for (int i = 0; i < 4; ++i) {
      bone_ids[i] = 0;
      bone_weights[i] = 0.0f;
    }
  }
};

glm::mat4 ConvertAssimpMatrix(const aiMatrix4x4 &m) {
  return Matrix((float)m.a1, (float)m.a2, (float)m.a3, (float)m.a4, (float)m.b1,
                (float)m.b2, (float)m.b3, (float)m.b4, (float)m.c1, (float)m.c2,
                (float)m.c3, (float)m.c4, (float)m.d1, (float)m.d2, (float)m.d3,
                (float)m.d4);
}

glm::vec3 ConvertAssimpVector(const aiVector3D &v) {
  return glm::vec3((float)v.x, (float)v.y, (float)v.z);
}

glm::quat ConvertAssimpQuaternion(const aiQuaternion &q) {
  return glm::normalize(
      glm::quat((float)q.w, (float)q.x, (float)q.y, (float)q.z));
}

void AddBoneInfluence(SalarymanAnimatedVertex &vertex, int bone_id,
                      float weight) {
  if (weight <= 0.0f)
    return;

  for (int i = 0; i < 4; ++i) {
    if (vertex.bone_weights[i] <= 0.0f) {
      vertex.bone_ids[i] = bone_id;
      vertex.bone_weights[i] = weight;
      return;
    }
  }

  int weakest_index = 0;
  for (int i = 1; i < 4; ++i) {
    if (vertex.bone_weights[i] < vertex.bone_weights[weakest_index])
      weakest_index = i;
  }

  if (weight > vertex.bone_weights[weakest_index]) {
    vertex.bone_ids[weakest_index] = bone_id;
    vertex.bone_weights[weakest_index] = weight;
  }
}

void NormalizeBoneInfluences(SalarymanAnimatedVertex &vertex) {
  float sum = 0.0f;
  for (int i = 0; i < 4; ++i)
    sum += vertex.bone_weights[i];

  if (sum <= 0.0001f)
    return;

  for (int i = 0; i < 4; ++i)
    vertex.bone_weights[i] /= sum;
}

glm::mat4 MakeQuaternionMatrix(glm::quat q) {
  q = glm::normalize(q);
  const float x = q.x;
  const float y = q.y;
  const float z = q.z;
  const float w = q.w;
  const float xx = x * x;
  const float yy = y * y;
  const float zz = z * z;
  const float xy = x * y;
  const float xz = x * z;
  const float yz = y * z;
  const float wx = w * x;
  const float wy = w * y;
  const float wz = w * z;

  return Matrix(1.0f - 2.0f * (yy + zz), 2.0f * (xy - wz), 2.0f * (xz + wy),
                0.0f, 2.0f * (xy + wz), 1.0f - 2.0f * (xx + zz),
                2.0f * (yz - wx), 0.0f, 2.0f * (xz - wy), 2.0f * (yz + wx),
                1.0f - 2.0f * (xx + yy), 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
}

int CopyAssimpNodeHierarchy(const aiNode *source_node,
                            SalarymanAnimatedModel &model) {
  const int node_index = (int)model.nodes.size();
  SalarymanAnimatedNode node;
  node.name = source_node->mName.C_Str();
  node.transform = ConvertAssimpMatrix(source_node->mTransformation);
  model.nodes.push_back(node);

  for (unsigned int i = 0; i < source_node->mNumChildren; ++i) {
    const int child_index =
        CopyAssimpNodeHierarchy(source_node->mChildren[i], model);
    model.nodes[node_index].children.push_back(child_index);
  }

  return node_index;
}

template <typename KeyType>
size_t FindAnimationKeyIndex(const std::vector<KeyType> &keys,
                             float animation_time) {
  if (keys.size() <= 1)
    return 0;

  for (size_t i = 0; i + 1 < keys.size(); ++i) {
    if (animation_time < keys[i + 1].time)
      return i;
  }

  return keys.size() - 2;
}

float InterpolationFactor(float animation_time, float current_time,
                          float next_time) {
  const float delta = next_time - current_time;
  if (std::fabs(delta) <= 0.0001f)
    return 0.0f;
  return std::max(0.0f,
                  std::min(1.0f, (animation_time - current_time) / delta));
}

glm::vec3 InterpolatePosition(const SalarymanAnimationChannel &channel,
                              float animation_time) {
  if (channel.positions.empty())
    return glm::vec3(0.0f, 0.0f, 0.0f);
  if (channel.positions.size() == 1)
    return channel.positions[0].value;

  const size_t index = FindAnimationKeyIndex(channel.positions, animation_time);
  const SalarymanPositionKey &current = channel.positions[index];
  const SalarymanPositionKey &next = channel.positions[index + 1];
  const float factor =
      InterpolationFactor(animation_time, current.time, next.time);
  return current.value + (next.value - current.value) * factor;
}

glm::quat InterpolateRotation(const SalarymanAnimationChannel &channel,
                              float animation_time) {
  if (channel.rotations.empty())
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  if (channel.rotations.size() == 1)
    return glm::normalize(channel.rotations[0].value);

  const size_t index = FindAnimationKeyIndex(channel.rotations, animation_time);
  const SalarymanRotationKey &current = channel.rotations[index];
  const SalarymanRotationKey &next = channel.rotations[index + 1];
  const float factor =
      InterpolationFactor(animation_time, current.time, next.time);
  glm::quat next_value = next.value;
  if (glm::dot(current.value, next_value) < 0.0f)
    next_value = -next_value;
  return glm::normalize(glm::slerp(current.value, next_value, factor));
}

glm::vec3 InterpolateScale(const SalarymanAnimationChannel &channel,
                           float animation_time) {
  if (channel.scales.empty())
    return glm::vec3(1.0f, 1.0f, 1.0f);
  if (channel.scales.size() == 1)
    return channel.scales[0].value;

  const size_t index = FindAnimationKeyIndex(channel.scales, animation_time);
  const SalarymanScaleKey &current = channel.scales[index];
  const SalarymanScaleKey &next = channel.scales[index + 1];
  const float factor =
      InterpolationFactor(animation_time, current.time, next.time);
  return current.value + (next.value - current.value) * factor;
}

const SalarymanAnimationChannel *
FindAnimationChannel(const SalarymanAnimation &animation,
                     const std::string &node_name) {
  std::map<std::string, int>::const_iterator it =
      animation.channelByNodeName.find(node_name);
  if (it == animation.channelByNodeName.end())
    return NULL;

  const int index = it->second;
  if (index < 0 || (size_t)index >= animation.channels.size())
    return NULL;

  return &animation.channels[(size_t)index];
}

glm::mat4 InterpolateNodeTransform(const SalarymanAnimationChannel &channel,
                                   float animation_time) {
  const glm::vec3 position = InterpolatePosition(channel, animation_time);
  const glm::quat rotation = InterpolateRotation(channel, animation_time);
  const glm::vec3 scale = InterpolateScale(channel, animation_time);

  return Matrix_Translate(position.x, position.y, position.z) *
         MakeQuaternionMatrix(rotation) *
         Matrix_Scale(scale.x, scale.y, scale.z);
}

bool IsSalarymanRootMotionChannel(const std::string &node_name);

float ComputeHorizontalRootMotionDistance(
    const SalarymanAnimationChannel &channel) {
  if (!IsSalarymanRootMotionChannel(channel.nodeName) ||
      channel.positions.size() <= 1)
    return 0.0f;

  const glm::vec3 first_position = channel.positions.front().value;
  const glm::vec3 last_position = channel.positions.back().value;
  const float delta_x = last_position.x - first_position.x;
  const float delta_z = last_position.z - first_position.z;
  return std::sqrt(delta_x * delta_x + delta_z * delta_z);
}

bool IsSalarymanRootMotionChannel(const std::string &node_name) {
  return node_name == "Hips" || node_name == "mixamorig7:Hips" ||
         node_name.find(":Hips") != std::string::npos;
}

void StripSalarymanHorizontalRootMotion(SalarymanAnimationChannel &channel) {
  if (!IsSalarymanRootMotionChannel(channel.nodeName) ||
      channel.positions.size() <= 1)
    return;

  const glm::vec3 first_position = channel.positions.front().value;
  const glm::vec3 last_position = channel.positions.back().value;
  const float delta_x = last_position.x - first_position.x;
  const float delta_z = last_position.z - first_position.z;

  if (std::fabs(delta_x) <= 0.0001f && std::fabs(delta_z) <= 0.0001f)
    return;

  for (size_t i = 0; i < channel.positions.size(); ++i) {
    channel.positions[i].value.x = first_position.x;
    channel.positions[i].value.z = first_position.z;
  }

  printf("Salaryman animation: stripped horizontal root motion from channel=%s "
         "originalDeltaXZ=(%.6f, %.6f)\n",
         channel.nodeName.c_str(), delta_x, delta_z);
}

void CalculateSalarymanBoneTransforms(SalarymanAnimatedModel &model,
                                       int node_index,
                                       const glm::mat4 &parent_transform,
                                       float animation_time,
                                       bool apply_animation) {
  if (node_index < 0 || (size_t)node_index >= model.nodes.size())
    return;

  const SalarymanAnimatedNode &node = model.nodes[(size_t)node_index];
  glm::mat4 node_transform = node.transform;
  if (apply_animation) {
    const SalarymanAnimationChannel *channel =
        FindAnimationChannel(model.animation, node.name);
    if (channel != NULL)
      node_transform = InterpolateNodeTransform(*channel, animation_time);
  }

  const glm::mat4 global_transform = parent_transform * node_transform;
  std::map<std::string, BoneInfo>::const_iterator bone_it =
      model.boneInfoMap.find(node.name);
  if (bone_it != model.boneInfoMap.end()) {
    const int bone_id = bone_it->second.id;
    if (bone_id >= 0 && bone_id < kMaxSalarymanBones &&
        (size_t)bone_id < model.finalBoneMatrices.size())
      model.finalBoneMatrices[(size_t)bone_id] = model.globalInverseTransform *
                                                 global_transform *
                                                 bone_it->second.offsetMatrix;
  }

  for (size_t i = 0; i < node.children.size(); ++i)
    CalculateSalarymanBoneTransforms(model, node.children[i], global_transform,
                                     animation_time, apply_animation);
}

void CalculateSalarymanBoneTransformsAtTime(SalarymanAnimatedModel &model,
                                             float animation_time,
                                             bool apply_animation = true) {
  if (model.rootNodeIndex < 0)
    return;

  for (size_t i = 0; i < model.finalBoneMatrices.size(); ++i)
    model.finalBoneMatrices[i] = Matrix_Identity();

  CalculateSalarymanBoneTransforms(model, model.rootNodeIndex,
                                   Matrix_Identity(), animation_time,
                                   apply_animation);
}
} // namespace

bool LoadSalarymanStaticModel(StaticModel &model, const char *filename) {
  model = StaticModel();
  const std::string path = ResolveExistingPath(filename);
  printf("Carregando modelo estatico do salaryman \"%s\"...\n", path.c_str());

  std::vector<unsigned char> bytes;
  if (!ReadWholeFile(path, bytes) || bytes.size() < 27) {
    fprintf(stderr, "ERROR: Cannot open FBX file \"%s\".\n", path.c_str());
    return false;
  }

  const char fbx_magic[] = "Kaydara FBX Binary";
  if (std::memcmp(bytes.data(), fbx_magic, sizeof(fbx_magic) - 1) != 0) {
    fprintf(stderr,
            "ERROR: Salaryman loader only supports binary FBX files.\n");
    return false;
  }

  const uint32_t version = ReadU32(bytes, 23);
  std::vector<SalarymanStaticVertex> raw_vertices;

  size_t offset = 27;
  while (offset < bytes.size()) {
    FbxNodeHeader node;
    if (!ReadFbxNodeHeader(bytes, version, offset, node))
      break;

    size_t prop_offset = offset;
    SkipFbxProperties(bytes, node.num_properties, prop_offset);

    if (node.name == "Objects") {
      size_t object_offset = prop_offset;
      while (object_offset <
             (size_t)node.end_offset - FbxNullRecordSize(version)) {
        FbxNodeHeader object_node;
        if (!ReadFbxNodeHeader(bytes, version, object_offset, object_node))
          break;

        size_t object_prop_offset = object_offset;
        if (object_node.name == "Geometry") {
          SkipFbxProperties(bytes, object_node.num_properties,
                            object_prop_offset);

          FbxMeshData mesh;
          if (ParseFbxGeometry(bytes, version, object_prop_offset,
                               object_node.end_offset, mesh))
            AppendFbxMeshAsStaticVertices(mesh, raw_vertices);
        }

        object_offset = (size_t)object_node.end_offset;
      }
    }

    offset = (size_t)node.end_offset;
  }

  if (raw_vertices.empty()) {
    fprintf(stderr, "ERROR: No static mesh geometry found in \"%s\".\n",
            path.c_str());
    return false;
  }

  glm::vec3 raw_min(std::numeric_limits<float>::max());
  glm::vec3 raw_max(std::numeric_limits<float>::lowest());
  for (size_t i = 0; i < raw_vertices.size(); ++i) {
    glm::vec3 p(raw_vertices[i].px, raw_vertices[i].py, raw_vertices[i].pz);
    raw_min.x = std::min(raw_min.x, p.x);
    raw_min.y = std::min(raw_min.y, p.y);
    raw_min.z = std::min(raw_min.z, p.z);
    raw_max.x = std::max(raw_max.x, p.x);
    raw_max.y = std::max(raw_max.y, p.y);
    raw_max.z = std::max(raw_max.z, p.z);
  }

  const float raw_height = std::max(1.0f, raw_max.y - raw_min.y);
  const float model_scale = 1.75f / raw_height;
  const glm::vec3 origin((raw_min.x + raw_max.x) * 0.5f, raw_min.y,
                         (raw_min.z + raw_max.z) * 0.5f);

  std::vector<GLuint> indices;
  indices.reserve(raw_vertices.size());
  glm::vec3 bbox_min(std::numeric_limits<float>::max());
  glm::vec3 bbox_max(std::numeric_limits<float>::lowest());

  for (size_t i = 0; i < raw_vertices.size(); ++i) {
    raw_vertices[i].px = (raw_vertices[i].px - origin.x) * model_scale;
    raw_vertices[i].py = (raw_vertices[i].py - origin.y) * model_scale;
    raw_vertices[i].pz = (raw_vertices[i].pz - origin.z) * model_scale;

    glm::vec3 p(raw_vertices[i].px, raw_vertices[i].py, raw_vertices[i].pz);
    bbox_min.x = std::min(bbox_min.x, p.x);
    bbox_min.y = std::min(bbox_min.y, p.y);
    bbox_min.z = std::min(bbox_min.z, p.z);
    bbox_max.x = std::max(bbox_max.x, p.x);
    bbox_max.y = std::max(bbox_max.y, p.y);
    bbox_max.z = std::max(bbox_max.z, p.z);
    indices.push_back((GLuint)i);
  }

  GLuint vertex_array_object_id;
  glGenVertexArrays(1, &vertex_array_object_id);
  glBindVertexArray(vertex_array_object_id);

  GLuint vertex_buffer_id;
  glGenBuffers(1, &vertex_buffer_id);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
  glBufferData(GL_ARRAY_BUFFER,
               raw_vertices.size() * sizeof(SalarymanStaticVertex),
               raw_vertices.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(SalarymanStaticVertex),
                        (void *)offsetof(SalarymanStaticVertex, px));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(SalarymanStaticVertex),
                        (void *)offsetof(SalarymanStaticVertex, nx));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SalarymanStaticVertex),
                        (void *)offsetof(SalarymanStaticVertex, u));
  glEnableVertexAttribArray(2);

  GLuint index_buffer_id;
  glGenBuffers(1, &index_buffer_id);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_id);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint),
               indices.data(), GL_STATIC_DRAW);

  const std::string object_name = "salaryman_static_model";
  SceneObject object;
  object.name = object_name;
  object.first_index = 0;
  object.num_indices = indices.size();
  object.rendering_mode = GL_TRIANGLES;
  object.vertex_array_object_id = vertex_array_object_id;
  object.bbox_min = bbox_min;
  object.bbox_max = bbox_max;
  g_VirtualScene[object_name] = object;

  glBindVertexArray(0);

  model.object_names.clear();
  model.object_names.push_back(object_name);
  model.bbox_min = bbox_min;
  model.bbox_max = bbox_max;
  model.loaded = true;

  printf("OK (%zu vertices estaticos).\n", raw_vertices.size());
  return true;
}

bool LoadTexturedAnimatedModel(SalarymanAnimatedModel &model,
                               const char *filename,
                               const char *body_diffuse_filename,
                               const char *hair_diffuse_filename,
                               const char *debug_label) {
  model = SalarymanAnimatedModel();
  if (debug_label == NULL)
    debug_label = "animated model";
  const std::string path = ResolveExistingPath(filename);
  printf("Loading animated %s FBX \"%s\"...\n", debug_label, path.c_str());

  Assimp::Importer importer;
  const unsigned int flags =
      aiProcess_Triangulate | aiProcess_GenSmoothNormals |
      aiProcess_LimitBoneWeights | aiProcess_ImproveCacheLocality |
      aiProcess_ValidateDataStructure;

  const aiScene *scene = importer.ReadFile(path, flags);
  if (scene == NULL || scene->mRootNode == NULL) {
    fprintf(stderr, "ERROR: Assimp failed to load animated %s: %s\n",
            debug_label, importer.GetErrorString());
    return false;
  }

  model.meshCount = (int)scene->mNumMeshes;
  model.animationCount = (int)scene->mNumAnimations;
  if (scene->mNumMeshes == 0 || scene->mNumAnimations == 0) {
    fprintf(stderr,
            "ERROR: Animated %s requires mesh and animation data. mesh "
            "count=%u animation count=%u\n",
            debug_label, scene->mNumMeshes, scene->mNumAnimations);
    return false;
  }

  GLuint body_diffuse_texture_id = 0;
  GLuint body_diffuse_texture_unit = 0;
  GLuint hair_diffuse_texture_id = 0;
  GLuint hair_diffuse_texture_unit = 0;
  const bool wants_hair_texture =
      hair_diffuse_filename != NULL && hair_diffuse_filename[0] != '\0';
  bool hair_texture_loaded = false;
  if (!LoadAnimatedDiffuseTexture(debug_label, body_diffuse_filename,
                                  body_diffuse_texture_id,
                                  body_diffuse_texture_unit))
    return false;
  if (wants_hair_texture &&
      LoadAnimatedDiffuseTexture(debug_label, hair_diffuse_filename,
                                 hair_diffuse_texture_id,
                                 hair_diffuse_texture_unit)) {
    hair_texture_loaded = true;
  } else {
    hair_diffuse_texture_id = body_diffuse_texture_id;
    hair_diffuse_texture_unit = body_diffuse_texture_unit;
    if (wants_hair_texture) {
      printf("%s animated FBX: hair diffuse texture unavailable, using "
             "body diffuse fallback.\n",
             debug_label);
    }
  }
  printf("%s animated FBX: body diffuse textureId=%u unit=%u, hair "
         "diffuse textureId=%u unit=%u\n",
         debug_label, body_diffuse_texture_id, body_diffuse_texture_unit,
         hair_diffuse_texture_id, hair_diffuse_texture_unit);

  aiMatrix4x4 root_inverse = scene->mRootNode->mTransformation;
  root_inverse.Inverse();
  model.globalInverseTransform = ConvertAssimpMatrix(root_inverse);
  model.rootNodeIndex = CopyAssimpNodeHierarchy(scene->mRootNode, model);

  glm::vec3 raw_min(std::numeric_limits<float>::max());
  glm::vec3 raw_max(std::numeric_limits<float>::lowest());
  bool found_vertex = false;

  for (unsigned int mesh_index = 0; mesh_index < scene->mNumMeshes;
       ++mesh_index) {
    const aiMesh *mesh = scene->mMeshes[mesh_index];
    if (mesh == NULL || mesh->mNumVertices == 0)
      continue;

    std::vector<SalarymanAnimatedVertex> vertices(mesh->mNumVertices);
    for (unsigned int vertex_index = 0; vertex_index < mesh->mNumVertices;
         ++vertex_index) {
      SalarymanAnimatedVertex &vertex = vertices[vertex_index];
      const aiVector3D &p = mesh->mVertices[vertex_index];
      vertex.px = (float)p.x;
      vertex.py = (float)p.y;
      vertex.pz = (float)p.z;
      vertex.pw = 1.0f;

      if (mesh->HasNormals()) {
        const aiVector3D &n = mesh->mNormals[vertex_index];
        glm::vec3 normal((float)n.x, (float)n.y, (float)n.z);
        if (glm::length(normal) > 0.0001f)
          normal = glm::normalize(normal);
        vertex.nx = normal.x;
        vertex.ny = normal.y;
        vertex.nz = normal.z;
        vertex.nw = 0.0f;
      }

      if (mesh->HasTextureCoords(0)) {
        const aiVector3D &uv = mesh->mTextureCoords[0][vertex_index];
        vertex.u = (float)uv.x;
        vertex.v = (float)uv.y;
      }

      glm::vec3 point(vertex.px, vertex.py, vertex.pz);
      raw_min.x = std::min(raw_min.x, point.x);
      raw_min.y = std::min(raw_min.y, point.y);
      raw_min.z = std::min(raw_min.z, point.z);
      raw_max.x = std::max(raw_max.x, point.x);
      raw_max.y = std::max(raw_max.y, point.y);
      raw_max.z = std::max(raw_max.z, point.z);
      found_vertex = true;
    }

    for (unsigned int bone_index = 0; bone_index < mesh->mNumBones;
         ++bone_index) {
      const aiBone *bone = mesh->mBones[bone_index];
      if (bone == NULL)
        continue;

      const std::string bone_name = bone->mName.C_Str();
      std::map<std::string, BoneInfo>::iterator bone_it =
          model.boneInfoMap.find(bone_name);
      if (bone_it == model.boneInfoMap.end()) {
        if (model.boneCount >= kMaxSalarymanBones) {
          fprintf(stderr,
                  "ERROR: Animated %s has more bones than supported "
                  "(%d).\n",
                  debug_label, kMaxSalarymanBones);
          return false;
        }

        BoneInfo bone_info;
        bone_info.id = model.boneCount;
        bone_info.offsetMatrix = ConvertAssimpMatrix(bone->mOffsetMatrix);
        bone_info.finalTransform = Matrix_Identity();
        bone_it = model.boneInfoMap.insert(std::make_pair(bone_name, bone_info))
                      .first;
        model.boneCount += 1;
      }

      const int bone_id = bone_it->second.id;
      for (unsigned int weight_index = 0; weight_index < bone->mNumWeights;
           ++weight_index) {
        const aiVertexWeight &weight = bone->mWeights[weight_index];
        if (weight.mVertexId < mesh->mNumVertices)
          AddBoneInfluence(vertices[weight.mVertexId], bone_id, weight.mWeight);
      }
    }

    for (size_t vertex_index = 0; vertex_index < vertices.size();
         ++vertex_index)
      NormalizeBoneInfluences(vertices[vertex_index]);

    std::vector<GLuint> indices;
    for (unsigned int face_index = 0; face_index < mesh->mNumFaces;
         ++face_index) {
      const aiFace &face = mesh->mFaces[face_index];
      if (face.mNumIndices != 3)
        continue;
      indices.push_back((GLuint)face.mIndices[0]);
      indices.push_back((GLuint)face.mIndices[1]);
      indices.push_back((GLuint)face.mIndices[2]);
    }

    if (indices.empty())
      continue;

    SalarymanAnimatedMesh animated_mesh;
    animated_mesh.name = mesh->mName.C_Str();
    animated_mesh.material_index = mesh->mMaterialIndex;
    if (mesh->mMaterialIndex < scene->mNumMaterials &&
        scene->mMaterials[mesh->mMaterialIndex] != NULL) {
      aiString material_name;
      if (AI_SUCCESS == scene->mMaterials[mesh->mMaterialIndex]->Get(
                            AI_MATKEY_NAME, material_name))
        animated_mesh.materialName = material_name.C_Str();
    }

    const bool mesh_requests_hair_texture =
        animated_mesh.name.find("Hair") != std::string::npos ||
        animated_mesh.name.find("hair") != std::string::npos ||
        animated_mesh.materialName.find("Hair") != std::string::npos ||
        animated_mesh.materialName.find("hair") != std::string::npos;
    const bool use_hair_texture =
        mesh_requests_hair_texture && hair_texture_loaded;
    animated_mesh.diffuse_texture_id =
        use_hair_texture ? hair_diffuse_texture_id : body_diffuse_texture_id;
    animated_mesh.diffuse_texture_unit = use_hair_texture
                                             ? hair_diffuse_texture_unit
                                             : body_diffuse_texture_unit;

    animated_mesh.num_indices = indices.size();
    animated_mesh.bbox_min = raw_min;
    animated_mesh.bbox_max = raw_max;

    glGenVertexArrays(1, &animated_mesh.vertex_array_object_id);
    glBindVertexArray(animated_mesh.vertex_array_object_id);

    glGenBuffers(1, &animated_mesh.vertex_buffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, animated_mesh.vertex_buffer_id);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(SalarymanAnimatedVertex),
                 vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE,
                          sizeof(SalarymanAnimatedVertex),
                          (void *)offsetof(SalarymanAnimatedVertex, px));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
                          sizeof(SalarymanAnimatedVertex),
                          (void *)offsetof(SalarymanAnimatedVertex, nx));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                          sizeof(SalarymanAnimatedVertex),
                          (void *)offsetof(SalarymanAnimatedVertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(3, 4, GL_INT, sizeof(SalarymanAnimatedVertex),
                           (void *)offsetof(SalarymanAnimatedVertex, bone_ids));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(
        4, 4, GL_FLOAT, GL_FALSE, sizeof(SalarymanAnimatedVertex),
        (void *)offsetof(SalarymanAnimatedVertex, bone_weights));
    glEnableVertexAttribArray(4);

    glGenBuffers(1, &animated_mesh.index_buffer_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, animated_mesh.index_buffer_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint),
                 indices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);

    printf(
        "%s animated mesh texture: mesh=%s material=%s assigned=%s "
        "textureId=%u unit=%u\n",
        debug_label,
        animated_mesh.name.empty() ? "<unnamed>" : animated_mesh.name.c_str(),
        animated_mesh.materialName.empty() ? "<unnamed>"
                                           : animated_mesh.materialName.c_str(),
        use_hair_texture ? hair_diffuse_filename : body_diffuse_filename,
        animated_mesh.diffuse_texture_id, animated_mesh.diffuse_texture_unit);

    model.meshes.push_back(animated_mesh);
  }

  if (!found_vertex || model.meshes.empty() || model.boneCount == 0) {
    fprintf(stderr,
            "ERROR: Animated %s missing usable mesh or bone data. mesh "
            "count=%u bone count=%d animation count=%u\n",
            debug_label, scene->mNumMeshes, model.boneCount,
            scene->mNumAnimations);
    return false;
  }

  const float raw_height = std::max(1.0f, raw_max.y - raw_min.y);
  const float model_scale = 1.75f / raw_height;
  const glm::vec3 origin((raw_min.x + raw_max.x) * 0.5f, raw_min.y,
                         (raw_min.z + raw_max.z) * 0.5f);

  const aiAnimation *animation = scene->mAnimations[0];
  model.animation.duration = (float)animation->mDuration;
  model.animation.ticksPerSecond = (animation->mTicksPerSecond > 0.0)
                                       ? (float)animation->mTicksPerSecond
                                       : 25.0f;
  if (model.animation.duration <= 0.0f) {
    fprintf(stderr,
            "ERROR: Animated %s animation has invalid duration.\n",
            debug_label);
    return false;
  }

  for (unsigned int channel_index = 0; channel_index < animation->mNumChannels;
       ++channel_index) {
    const aiNodeAnim *ai_channel = animation->mChannels[channel_index];
    if (ai_channel == NULL)
      continue;

    SalarymanAnimationChannel channel;
    channel.nodeName = ai_channel->mNodeName.C_Str();

    for (unsigned int i = 0; i < ai_channel->mNumPositionKeys; ++i) {
      SalarymanPositionKey key;
      key.time = (float)ai_channel->mPositionKeys[i].mTime;
      key.value = ConvertAssimpVector(ai_channel->mPositionKeys[i].mValue);
      channel.positions.push_back(key);
    }

    for (unsigned int i = 0; i < ai_channel->mNumRotationKeys; ++i) {
      SalarymanRotationKey key;
      key.time = (float)ai_channel->mRotationKeys[i].mTime;
      key.value = ConvertAssimpQuaternion(ai_channel->mRotationKeys[i].mValue);
      channel.rotations.push_back(key);
    }

    for (unsigned int i = 0; i < ai_channel->mNumScalingKeys; ++i) {
      SalarymanScaleKey key;
      key.time = (float)ai_channel->mScalingKeys[i].mTime;
      key.value = ConvertAssimpVector(ai_channel->mScalingKeys[i].mValue);
      channel.scales.push_back(key);
    }

    if (IsSalarymanRootMotionChannel(channel.nodeName)) {
      model.rootMotionDistance =
          ComputeHorizontalRootMotionDistance(channel) * model_scale;
    }

    StripSalarymanHorizontalRootMotion(channel);

    const int stored_index = (int)model.animation.channels.size();
    model.animation.channelByNodeName[channel.nodeName] = stored_index;
    model.animation.channels.push_back(channel);
  }

  if (model.animation.channels.empty()) {
    fprintf(stderr, "ERROR: Animated %s has no animation channels.\n",
            debug_label);
    return false;
  }

  model.animationDurationSeconds =
      model.animation.duration / model.animation.ticksPerSecond;
  if (model.animationDurationSeconds > 0.0001f)
    model.recommendedWalkSpeed =
        model.rootMotionDistance / model.animationDurationSeconds;

  model.normalizationMatrix =
      Matrix_Scale(model_scale, model_scale, model_scale) *
      Matrix_Translate(-origin.x, -origin.y, -origin.z);

  model.finalBoneMatrices.assign(kMaxSalarymanBones, Matrix_Identity());
  CalculateSalarymanBoneTransformsAtTime(model, 0.0f);
  model.loaded = true;

  printf("%s animated FBX: mesh count=%d\n", debug_label, model.meshCount);
  printf("%s animated FBX: bone count=%d\n", debug_label, model.boneCount);
  printf("%s animated FBX: animation count=%d\n", debug_label,
         model.animationCount);
  printf("%s animated FBX: animation duration=%.3f\n", debug_label,
         model.animation.duration);
  printf("%s animated FBX: ticks per second=%.3f\n", debug_label,
         model.animation.ticksPerSecond);
  printf("%s animated FBX: normalized root stride=%.3f, cycle seconds=%.3f, "
         "recommended walk speed=%.3f\n",
         debug_label, model.rootMotionDistance,
         model.animationDurationSeconds, model.recommendedWalkSpeed);
  return true;
}

bool LoadSalarymanAnimatedModel(SalarymanAnimatedModel &model,
                                const char *filename) {
  return LoadTexturedAnimatedModel(
      model, filename, "assets/salaryman/Ch33_1001_Diffuse.png",
      "assets/salaryman/Ch33_1002_Diffuse.png", "salaryman");
}

void SetAnimatedModelToBindPose(SalarymanAnimatedModel &model) {
  CalculateSalarymanBoneTransformsAtTime(model, 0.0f, false);
}

void UpdateSalarymanAnimation(SalarymanAnimator &animator, float delta_time) {
  if (animator.model == NULL || !animator.model->loaded)
    return;

  SalarymanAnimatedModel &model = *animator.model;
  if (model.animation.duration <= 0.0f)
    return;

  const float ticks_per_second = (model.animation.ticksPerSecond > 0.0f)
                                     ? model.animation.ticksPerSecond
                                     : 25.0f;
  animator.currentTime += ticks_per_second * delta_time;
  animator.currentTime =
      std::fmod(animator.currentTime, model.animation.duration);
  if (animator.currentTime < 0.0f)
    animator.currentTime += model.animation.duration;

  CalculateSalarymanBoneTransformsAtTime(model, animator.currentTime);
}

void DrawAnimatedModel(const SalarymanAnimatedModel &model) {
  if (!model.loaded || model.meshes.empty())
    return;

  if (g_use_skinning_uniform >= 0)
    glUniform1i(g_use_skinning_uniform, GL_TRUE);

  if (g_bone_matrices_uniform >= 0 && !model.finalBoneMatrices.empty()) {
    const GLsizei count = (GLsizei)std::min(model.finalBoneMatrices.size(),
                                            (size_t)kMaxSalarymanBones);
    glUniformMatrix4fv(g_bone_matrices_uniform, count, GL_FALSE,
                       glm::value_ptr(model.finalBoneMatrices[0]));
  }

  for (size_t i = 0; i < model.meshes.size(); ++i) {
    const SalarymanAnimatedMesh &mesh = model.meshes[i];
    glBindVertexArray(mesh.vertex_array_object_id);
    if (mesh.diffuse_texture_id != 0) {
      glActiveTexture(GL_TEXTURE0 + mesh.diffuse_texture_unit);
      glBindTexture(GL_TEXTURE_2D, mesh.diffuse_texture_id);
    }
    glUniform1i(g_material_diffuse_uniform, mesh.diffuse_texture_unit);
    glUniform4f(g_bbox_min_uniform, mesh.bbox_min.x, mesh.bbox_min.y,
                mesh.bbox_min.z, 1.0f);
    glUniform4f(g_bbox_max_uniform, mesh.bbox_max.x, mesh.bbox_max.y,
                mesh.bbox_max.z, 1.0f);
    glDrawElements(GL_TRIANGLES, (GLsizei)mesh.num_indices, GL_UNSIGNED_INT, 0);
  }

  glBindVertexArray(0);

  if (g_use_skinning_uniform >= 0)
    glUniform1i(g_use_skinning_uniform, GL_FALSE);
}

void DrawStaticModel(const StaticModel &model) {
  for (size_t i = 0; i < model.object_names.size(); ++i)
    DrawVirtualObject(model.object_names[i].c_str());
}
