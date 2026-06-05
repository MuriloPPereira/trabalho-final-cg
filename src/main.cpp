//     Universidade Federal do Rio Grande do Sul
//             Instituto de Informática
//       Departamento de Informática Aplicada
//
//    INF01047 Computação Gráfica e Visualização I
//               Prof. Eduardo Gastal
//
//     CÓDIGO BASE PARA O TRABALHO FINAL
//

// Arquivos "headers" padrões de C podem ser incluídos em um
// programa C++, sendo necessário somente adicionar o caractere
// "c" antes de seu nome, e remover o sufixo ".h". Exemplo:
//    #include <stdio.h> // Em C
//  vira
//    #include <cstdio> // Em C++
//
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Headers abaixo são específicos de C++
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <vector>

// Headers das bibliotecas OpenGL
#include <glad/glad.h>  // Criação de contexto OpenGL 3.3
#include <GLFW/glfw3.h> // Criação de janelas do sistema operacional

// Headers da biblioteca GLM: criação de matrizes e vetores.
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

// Headers da biblioteca para carregar modelos obj
#include <tiny_obj_loader.h>

#include <stb_image.h>

// Headers locais, definidos na pasta "include/"
#include "collisions.h"
#include "matrices.h"
#include "utils.h"
#include <iostream>

// Estrutura que representa um modelo geométrico carregado a partir de um
// arquivo ".obj". Veja https://en.wikipedia.org/wiki/Wavefront_.obj_file .
#ifndef ENABLE_CORRIDOR_DEBUG_LOGS
#define ENABLE_CORRIDOR_DEBUG_LOGS 0
#endif

static const bool kCorridorDebugLogsEnabled = (ENABLE_CORRIDOR_DEBUG_LOGS != 0);

struct ObjModel {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  // Este construtor lê o modelo de um arquivo utilizando a biblioteca
  // tinyobjloader. Veja: https://github.com/syoyo/tinyobjloader
  ObjModel(const char *filename, const char *basepath = NULL,
           bool triangulate = true) {
    printf("Carregando objetos do arquivo \"%s\"...\n", filename);

    // Se basepath == NULL, então setamos basepath como o dirname do
    // filename, para que os arquivos MTL sejam corretamente carregados caso
    // estejam no mesmo diretório dos arquivos OBJ.
    std::string fullpath(filename);
    std::string dirname;
    if (basepath == NULL) {
      auto i = fullpath.find_last_of("/");
      if (i != std::string::npos) {
        dirname = fullpath.substr(0, i + 1);
        basepath = dirname.c_str();
      }
    }

    std::string warn;
    std::string err;
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                                filename, basepath, triangulate);

    if (!err.empty())
      fprintf(stderr, "\n%s\n", err.c_str());

    if (!ret)
      throw std::runtime_error("Erro ao carregar modelo.");

    for (size_t shape = 0; shape < shapes.size(); ++shape) {
      if (shapes[shape].name.empty()) {
        fprintf(stderr,
                "*********************************************\n"
                "Erro: Objeto sem nome dentro do arquivo '%s'.\n"
                "Veja "
                "https://www.inf.ufrgs.br/~eslgastal/"
                "fcg-faq-etc.html#Modelos-3D-no-formato-OBJ .\n"
                "*********************************************\n",
                filename);
        throw std::runtime_error("Objeto sem nome.");
      }
      printf("- Objeto '%s'\n", shapes[shape].name.c_str());
    }

    printf("OK.\n");
  }
};

// Declaração de funções utilizadas para pilha de matrizes de modelagem.
void PushMatrix(glm::mat4 M);
void PopMatrix(glm::mat4 &M);

struct Material;
struct PointLight;
struct StaticModel;
struct SalarymanAnimatedModel;
struct SalarymanAnimator;
struct SalarymanNPC;
struct CorridorContentFrame;
struct CorridorContent;

// Declaração de várias funções utilizadas em main().  Essas estão definidas
// logo após a definição de main() neste arquivo.
void BuildTrianglesAndAddToVirtualScene(
    ObjModel *); // Constrói representação de um ObjModel como malha de
                 // triângulos para renderização
void BuildCorridorAndAddToVirtualScene(); // Constrói um corredor procedural
                                          // simples (chão, teto e paredes)
void BuildCornerAndAddToVirtualScene();  // Constrói duas quinas procedurais 4x4
                                         // (esquerda e direita)
void BuildPostersAndAddToVirtualScene(); // Constrói quads para os posters na
                                         // parede esquerda
void ComputeNormals(
    ObjModel *model); // Computa normais de um ObjModel, caso não existam.
void LoadShadersFromFiles(); // Carrega os shaders de vértice e fragmento,
                             // criando um programa de GPU
void LoadTextureImage(const char *filename, GLint wrap_s,
                      GLint wrap_t); // Função que carrega imagens de textura
void DrawVirtualObject(
    const char *object_name); // Desenha um objeto armazenado em g_VirtualScene
void ApplyMaterial(const struct Material &material);
void SetPointLights(const std::vector<struct PointLight> &lights);
void CreateSolidColorTexture(unsigned char r, unsigned char g, unsigned char b);
bool LoadSalarymanStaticModel(StaticModel &model, const char *filename);
bool LoadSalarymanAnimatedModel(SalarymanAnimatedModel &model,
                                const char *filename);
void SpawnSalarymanForCorridor(SalarymanNPC &salaryman,
                               const CorridorContent &content,
                               const glm::vec3 &player_position);
void UpdateSalarymanNPC(SalarymanNPC &salaryman, float delta_time,
                        const glm::vec4 &camera_position_c);
void UpdateSalarymanAnimation(SalarymanAnimator &animator, float delta_time);
void DrawStaticModel(const StaticModel &model);
void DrawAnimatedModel(const SalarymanAnimatedModel &model);
void DrawSalarymanNPC(const SalarymanNPC &salaryman, const Material &material);
GLuint LoadShader_Vertex(const char *filename);   // Carrega um vertex shader
GLuint LoadShader_Fragment(const char *filename); // Carrega um fragment shader
void LoadShader(const char *filename,
                GLuint shader_id); // Função utilizada pelas duas acima
GLuint CreateGpuProgram(GLuint vertex_shader_id,
                        GLuint fragment_shader_id); // Cria um programa de GPU
void PrintObjModelInfo(ObjModel *);                 // Função para debugging

// Declaração de funções auxiliares para renderizar texto dentro da janela
// OpenGL. Estas funções estão definidas no arquivo "textrendering.cpp".
void TextRendering_Init();
float TextRendering_LineHeight(GLFWwindow *window);
float TextRendering_CharWidth(GLFWwindow *window);
void TextRendering_PrintString(GLFWwindow *window, const std::string &str,
                               float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrix(GLFWwindow *window, glm::mat4 M, float x,
                               float y, float scale = 1.0f);
void TextRendering_PrintVector(GLFWwindow *window, glm::vec4 v, float x,
                               float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProduct(GLFWwindow *window, glm::mat4 M,
                                            glm::vec4 v, float x, float y,
                                            float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductMoreDigits(GLFWwindow *window,
                                                      glm::mat4 M, glm::vec4 v,
                                                      float x, float y,
                                                      float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductDivW(GLFWwindow *window, glm::mat4 M,
                                                glm::vec4 v, float x, float y,
                                                float scale = 1.0f);

// Funções abaixo renderizam como texto na janela OpenGL algumas matrizes e
// outras informações do programa. Definidas após main().
void TextRendering_ShowModelViewProjection(GLFWwindow *window,
                                           glm::mat4 projection, glm::mat4 view,
                                           glm::mat4 model, glm::vec4 p_model);
void TextRendering_ShowEulerAngles(GLFWwindow *window);
void TextRendering_ShowProjection(GLFWwindow *window);
void TextRendering_ShowFramesPerSecond(GLFWwindow *window);

// Funções callback para comunicação com o sistema operacional e interação do
// usuário. Veja mais comentários nas definições das mesmas, abaixo.
void FramebufferSizeCallback(GLFWwindow *window, int width, int height);
void ErrorCallback(int error, const char *description);
void KeyCallback(GLFWwindow *window, int key, int scancode, int action,
                 int mode);
void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
void CursorPosCallback(GLFWwindow *window, double xpos, double ypos);
void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset);
void UpdateCameraFromInput(GLFWwindow *window, float delta_time);
glm::vec4 ComputeCameraFrontVector();

// Definimos uma estrutura que armazenará dados necessários para renderizar
// cada objeto da cena virtual.
struct SceneObject {
  std::string name;   // Nome do objeto
  size_t first_index; // Índice do primeiro vértice dentro do vetor indices[]
                      // definido em BuildTrianglesAndAddToVirtualScene()
  size_t num_indices; // Número de índices do objeto dentro do vetor indices[]
                      // definido em BuildTrianglesAndAddToVirtualScene()
  GLenum rendering_mode;         // Modo de rasterização (GL_TRIANGLES,
                                 // GL_TRIANGLE_STRIP, etc.)
  GLuint vertex_array_object_id; // ID do VAO onde estão armazenados os
                                 // atributos do modelo
  glm::vec3 bbox_min;            // Axis-Aligned Bounding Box do objeto
  glm::vec3 bbox_max;
};

struct Material {
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

struct StaticModel {
  std::vector<std::string> object_names;
  glm::vec3 bbox_min;
  glm::vec3 bbox_max;
};

const int kMaxSalarymanBones = 100;

struct BoneInfo {
  int id;
  glm::mat4 offsetMatrix;
  glm::mat4 finalTransform;

  BoneInfo()
      : id(-1), offsetMatrix(Matrix_Identity()),
        finalTransform(Matrix_Identity()) {}
};

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

  SalarymanAnimatedMesh()
      : name(), materialName(), vertex_array_object_id(0), vertex_buffer_id(0),
        index_buffer_id(0), diffuse_texture_id(0), diffuse_texture_unit(0),
        material_index(0), num_indices(0), bbox_min(0.0f, 0.0f, 0.0f),
        bbox_max(0.0f, 0.0f, 0.0f) {}
};

struct SalarymanPositionKey {
  float time;
  glm::vec3 value;
};

struct SalarymanRotationKey {
  float time;
  glm::quat value;
};

struct SalarymanScaleKey {
  float time;
  glm::vec3 value;
};

struct SalarymanAnimationChannel {
  std::string nodeName;
  std::vector<SalarymanPositionKey> positions;
  std::vector<SalarymanRotationKey> rotations;
  std::vector<SalarymanScaleKey> scales;
};

struct SalarymanAnimation {
  float duration;
  float ticksPerSecond;
  std::vector<SalarymanAnimationChannel> channels;
  std::map<std::string, int> channelByNodeName;

  SalarymanAnimation() : duration(0.0f), ticksPerSecond(25.0f) {}
};

struct SalarymanAnimatedNode {
  std::string name;
  glm::mat4 transform;
  std::vector<int> children;

  SalarymanAnimatedNode() : transform(Matrix_Identity()) {}
};

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

  SalarymanAnimatedModel()
      : loaded(false), boneCount(0), meshCount(0), animationCount(0),
        globalInverseTransform(Matrix_Identity()),
        normalizationMatrix(Matrix_Identity()), rootNodeIndex(-1) {}
};

struct SalarymanAnimator {
  SalarymanAnimatedModel *model;
  float currentTime;

  SalarymanAnimator() : model(NULL), currentTime(0.0f) {}
};

struct SalarymanNPC {
  bool active;
  int corridorId;
  glm::vec3 position;
  glm::vec3 forward;
  float speed;
  float corridorLength;
  glm::vec3 corridorOrigin;
  bool useAnimation;
  StaticModel *model;
  SalarymanAnimatedModel *animatedModel;
  SalarymanAnimator *animator;

  SalarymanNPC()
      : active(false), corridorId(-1), position(0.0f, 0.0f, 0.0f),
        forward(0.0f, 0.0f, -1.0f), speed(1.35f), corridorLength(0.0f),
        corridorOrigin(0.0f, 0.0f, 0.0f), useAnimation(false), model(NULL),
        animatedModel(NULL), animator(NULL) {}
};

// Abaixo definimos variáveis globais utilizadas em várias funções do código.

// A cena virtual é uma lista de objetos nomeados, guardados em um dicionário
// (map).  Veja dentro da função BuildTrianglesAndAddToVirtualScene() como que
// são incluídos objetos dentro da variável g_VirtualScene, e veja na função
// main() como estes são acessados.
std::map<std::string, SceneObject> g_VirtualScene;
StaticModel g_SalarymanStaticModel;
SalarymanAnimatedModel g_SalarymanAnimatedModel;
SalarymanAnimator g_SalarymanAnimator;
SalarymanNPC g_SalarymanNPC;

// Pilha que guardará as matrizes de modelagem.
std::stack<glm::mat4> g_MatrixStack;

// Razão de proporção da janela (largura/altura). Veja função
// FramebufferSizeCallback().
float g_ScreenRatio = 1.0f;

// Ângulos de Euler que controlam a rotação de um dos cubos da cena virtual
float g_AngleX = 0.0f;
float g_AngleY = 0.0f;
float g_AngleZ = 0.0f;

// "g_LeftMouseButtonPressed = true" se o usuário está com o botão esquerdo do
// mouse pressionado no momento atual. Veja função MouseButtonCallback().
bool g_LeftMouseButtonPressed = false;
bool g_RightMouseButtonPressed = false;  // Análogo para botão direito do mouse
bool g_MiddleMouseButtonPressed = false; // Análogo para botão do meio do mouse

// Câmera em primeira pessoa.
glm::vec4 g_CameraPosition = glm::vec4(0.0f, 1.6f, -1.0f, 1.0f);
float g_CameraYaw = 0.0f;
float g_CameraPitch = 0.0f;
bool g_FirstMouseInput = true;

struct CorridorState {
  int id;
  bool has_anomaly;
};

struct CorridorRenderTransform {
  glm::mat4 geometryFromLocal;
};

struct CorridorContentFrame {
  int logicalCorridorId;
  glm::vec3 contentOrigin;
  glm::vec3 contentForward;
  glm::vec3 contentRight;
  float corridorLength;
  float connectorLength;
  const char *posterWallSide;
};

struct PosterSlotLayout {
  int slot;
  int textureIndex;
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 up;
  glm::vec3 widthAxis;
  const char *wallSide;
};

struct CorridorContent {
  int corridorId;
  CorridorContentFrame frame;
  std::vector<PosterSlotLayout> posters;
  std::vector<glm::vec3> lightPositions;
  glm::vec3 salarymanSpawnPosition;
  glm::vec3 salarymanForward;
  bool hasAnomaly;
};

struct CorridorInstance {
  CorridorState state;
  CorridorContent content;
};

int g_CurrentExitLevel = 0;

int g_CurrentCorridorSequenceId = 0;
int g_NextCorridorSequenceId = 1;
int g_LastEnteredPhysicalSide = 0;
int g_LastSalarymanSpawnCorridorId = -1;
int g_PreparedNextCorridorId = -1;
int g_PreparedTransitionDirection = 0;
bool g_InConnectorTransition = false;
bool g_ConnectorMidpointCrossed = false;
int g_LastPlayerSection = -1;
CorridorInstance g_CurrentCorridorInstance;
CorridorInstance g_NegativeCandidateCorridorInstance;
CorridorInstance g_PositiveCandidateCorridorInstance;

const int kPosterCount = 4;

int PositiveModulo(int value, int divisor) {
  int result = value % divisor;
  return (result < 0) ? result + divisor : result;
}

int GetPosterTextureIndex(int corridor_id, int poster_slot) {
  (void)corridor_id;
  return PositiveModulo(poster_slot, kPosterCount);
}

CorridorContent GenerateCorridorContent(int corridor_id,
                                        const glm::vec3 &content_forward,
                                        bool has_anomaly);
CorridorInstance CreateNewCorridorInstance(int logical_id,
                                           const glm::vec3 &content_forward,
                                           bool has_anomaly);

CorridorState MakeCorridorState(int id) {
  CorridorState state;
  state.id = id;
  state.has_anomaly = false; // Hook for later anomaly selection.
  return state;
}

void RefreshCandidateCorridorStates() {

  bool next_has_anomaly = (rand() % 2 == 0);

  g_NegativeCandidateCorridorInstance = CreateNewCorridorInstance(
      g_NextCorridorSequenceId, glm::vec3(0.0f, 0.0f, +1.0f), next_has_anomaly);
  g_PositiveCandidateCorridorInstance = CreateNewCorridorInstance(
      g_NextCorridorSequenceId, glm::vec3(0.0f, 0.0f, -1.0f), next_has_anomaly);

  g_NegativeCandidateCorridorInstance.state.has_anomaly = next_has_anomaly;
  g_NegativeCandidateCorridorInstance.content.hasAnomaly = next_has_anomaly;

  g_PositiveCandidateCorridorInstance.state.has_anomaly = next_has_anomaly;
  g_PositiveCandidateCorridorInstance.content.hasAnomaly = next_has_anomaly;
}
void InitializeCorridorLifecycle() {
  g_CurrentCorridorSequenceId = 0;
  g_NextCorridorSequenceId = 1;
  g_CurrentCorridorInstance = CreateNewCorridorInstance(
      g_CurrentCorridorSequenceId, glm::vec3(0.0f, 0.0f, -1.0f), false);
  RefreshCandidateCorridorStates();
}

void ActivateNewLogicalCorridor(int physical_side) {
  if (physical_side == 0)
    return;


  float player_moved_z = -(float)physical_side;

  // O jogador seguiu em frente se ele andou na mesma direção que a frente do corredor aponta
  bool went_forward = (player_moved_z == g_CurrentCorridorInstance.content.frame.contentForward.z);
  bool had_anomaly = g_CurrentCorridorInstance.state.has_anomaly;

  bool is_correct =
      (went_forward && !had_anomaly) || (!went_forward && had_anomaly);

  if (is_correct) {
    g_CurrentExitLevel++;
    printf("\n--- CORRECT -> EXIT LEVEL: %d ---\n\n", g_CurrentExitLevel);
  } else {
    g_CurrentExitLevel = 0;
    printf("\n--- INCORRECT -> EXIT LEVEL: %d ---\n\n", g_CurrentExitLevel);
  }

  if (g_CurrentExitLevel >= 8)
    printf("\n*** YOU WIN! You reached Exit 8! ***\n\n");

  fflush(stdout);

  g_LastEnteredPhysicalSide = physical_side;
  g_CurrentCorridorInstance = (physical_side < 0)
                                  ? g_NegativeCandidateCorridorInstance
                                  : g_PositiveCandidateCorridorInstance;

  if (g_CurrentCorridorInstance.state.has_anomaly) {
    printf("\n[SPOILER] ANOMALIA NO CORREDOR ATUAL (POSTERS IDENTICOS)\n");
  } else {
    printf("\n[SPOILER] CORREDOR NORMAL\n");
  }
  fflush(stdout);
  g_CurrentCorridorSequenceId = g_CurrentCorridorInstance.state.id;
  g_NextCorridorSequenceId = g_CurrentCorridorSequenceId + 1;
  RefreshCandidateCorridorStates();
}

// Variáveis que controlam rotação do antebraço
float g_ForearmAngleZ = 0.0f;
float g_ForearmAngleX = 0.0f;

// Variáveis que controlam translação do torso
float g_TorsoPositionX = 0.0f;
float g_TorsoPositionY = 0.0f;

// Variável que controla o tipo de projeção utilizada: perspectiva ou
// ortográfica.
bool g_UsePerspectiveProjection = true;

// Variável que controla se o texto informativo será mostrado na tela.
bool g_ShowInfoText = true;

// Variáveis que definem um programa de GPU (shaders). Veja função
// LoadShadersFromFiles().
GLuint g_GpuProgramID = 0;
GLint g_model_uniform;
GLint g_view_uniform;
GLint g_projection_uniform;
GLint g_use_skinning_uniform;
GLint g_bone_matrices_uniform;
GLint g_bbox_min_uniform;
GLint g_bbox_max_uniform;
GLint g_camera_position_uniform;
GLint g_material_diffuse_uniform;
GLint g_material_specular_strength_uniform;
GLint g_material_shininess_uniform;
GLint g_material_ambient_strength_uniform;
GLint g_material_uv_scale_uniform;
GLint g_material_uv_offset_uniform;
GLint g_num_lights_uniform;

const int kMaxLights = 30;
GLint g_light_position_uniforms[kMaxLights];
GLint g_light_color_uniforms[kMaxLights];
GLint g_light_ambient_strength_uniforms[kMaxLights];
GLint g_light_diffuse_strength_uniforms[kMaxLights];
GLint g_light_specular_strength_uniforms[kMaxLights];
GLint g_light_constant_uniforms[kMaxLights];
GLint g_light_linear_uniforms[kMaxLights];
GLint g_light_quadratic_uniforms[kMaxLights];

const float kCorridorHalfWidth = 2.0f;
const float kCorridorHeight = 3.0f;
const float kCorridorLength = 40.0f;
const float kCorridorZ0 = 0.0f;
const float kCorridorZ1 = -kCorridorLength;
const float kCornerLength = 4.0f;
const float kConnectorLength = kCorridorLength * 0.5f;
const float kFloorTileSize = 0.5f;
const float kCeilingTileSize = 2.5f;
const float kWallTextureTileSize = 2.0f;
const char *kPosterNames[kPosterCount] = {"poster_0", "poster_1", "poster_2",
                                          "poster_3"};

CanonicalCorridorLayout GetCanonicalCorridorLayout() {
  CanonicalCorridorLayout layout;
  layout.connector_length = kConnectorLength;
  layout.turn_z0 = kCorridorZ1;
  layout.turn_z1 = layout.turn_z0 - kCornerLength;
  layout.connector_center_z = layout.turn_z0 - 0.5f * kCornerLength;
  layout.connector_start_x = -kCorridorHalfWidth;
  layout.connector_end_x = layout.connector_start_x - layout.connector_length;
  layout.exit_turn_x = layout.connector_end_x - kCorridorHalfWidth;
  layout.corridor2_offset_x = layout.exit_turn_x;
  layout.second_corridor_z_offset = layout.turn_z1;
  layout.block_offset =
      glm::vec2(layout.corridor2_offset_x, layout.second_corridor_z_offset);
  return layout;
}

glm::vec3 TransformPoint(const glm::mat4 &transform, const glm::vec3 &point) {
  glm::vec4 p = transform * glm::vec4(point.x, point.y, point.z, 1.0f);
  return glm::vec3(p.x, p.y, p.z);
}

glm::vec3 TransformVector(const glm::mat4 &transform, const glm::vec3 &vector) {
  glm::vec4 v = transform * glm::vec4(vector.x, vector.y, vector.z, 0.0f);
  return glm::vec3(v.x, v.y, v.z);
}

CorridorRenderTransform
MakeCorridorRenderTransform(const glm::mat4 &geometry_from_local) {
  CorridorRenderTransform transform;
  transform.geometryFromLocal = geometry_from_local;
  return transform;
}

CorridorContentFrame
MakeCorridorContentFrame(int logical_corridor_id,
                         const glm::vec3 &requested_content_forward) {
  const CanonicalCorridorLayout layout = GetCanonicalCorridorLayout();
  const glm::vec3 world_up(0.0f, 1.0f, 0.0f);

  CorridorContentFrame frame;
  frame.logicalCorridorId = logical_corridor_id;
  frame.contentForward = requested_content_forward;
  if (glm::length(frame.contentForward) < 0.0001f)
    frame.contentForward = glm::vec3(0.0f, 0.0f, -1.0f);
  else
    frame.contentForward = glm::normalize(frame.contentForward);

  frame.contentRight = glm::cross(frame.contentForward, world_up);
  if (glm::length(frame.contentRight) < 0.0001f)
    frame.contentRight = glm::vec3(1.0f, 0.0f, 0.0f);
  else
    frame.contentRight = glm::normalize(frame.contentRight);

  frame.contentOrigin =
      (glm::dot(frame.contentForward, glm::vec3(0.0f, 0.0f, -1.0f)) >= 0.0f)
          ? glm::vec3(0.0f, 0.0f, kCorridorZ0)
          : glm::vec3(0.0f, 0.0f, kCorridorZ1);
  frame.corridorLength = kCorridorLength;
  frame.connectorLength = layout.connector_length;
  frame.posterWallSide = "canonical_left";
  return frame;
}

CorridorContent GenerateCorridorContent(int corridor_id,
                                        const glm::vec3 &content_forward,
                                        bool has_anomaly) {
  const float poster_center_y = 1.6f;
  const float poster_offset = 0.02f;
  const float poster_wall_offset = kCorridorHalfWidth - poster_offset;
  const float spacing = kCorridorLength / (kPosterCount + 1);
  const float salaryman_spawn_distance = kCorridorLength * 0.70f;
  const float salaryman_end_margin = 6.0f;

  CorridorContent content;
  content.corridorId = corridor_id;
  content.frame = MakeCorridorContentFrame(corridor_id, content_forward);
  content.posters.reserve(kPosterCount);
  content.lightPositions.reserve(7);
  content.hasAnomaly = false;

  const CorridorContentFrame &frame = content.frame;
  for (int slot = 0; slot < kPosterCount; ++slot) {
    const float poster_distance = (slot + 1) * spacing;

    PosterSlotLayout poster;
    poster.slot = slot;
    if (has_anomaly) {
      poster.textureIndex = 0;
    } else {
      poster.textureIndex = GetPosterTextureIndex(corridor_id, slot);
    }
    poster.position = frame.contentOrigin -
                      frame.contentRight * poster_wall_offset +
                      frame.contentForward * poster_distance;
    poster.position.y = poster_center_y;
    poster.normal = frame.contentRight;
    poster.up = glm::vec3(0.0f, 1.0f, 0.0f);
    poster.widthAxis = -frame.contentForward;
    poster.wallSide = frame.posterWallSide;
    content.posters.push_back(poster);
  }

  const CanonicalCorridorLayout layout = GetCanonicalCorridorLayout();
  const float light_y = kCorridorHeight - 0.15f;
  const float straight_light_spacing = kCorridorLength / 5.0f;
  for (int i = 0; i < 4; ++i) {
    glm::vec3 light_position =
        frame.contentOrigin +
        frame.contentForward * ((i + 1) * straight_light_spacing);
    light_position.y = light_y;
    content.lightPositions.push_back(light_position);
  }
  content.lightPositions.push_back(
      glm::vec3(0.0f, light_y, layout.turn_z0 - 0.5f * kCornerLength));
  content.lightPositions.push_back(
      glm::vec3(layout.connector_start_x - 0.5f * layout.connector_length,
                light_y, layout.connector_center_z));
  content.lightPositions.push_back(glm::vec3(
      layout.exit_turn_x, light_y, layout.turn_z0 - 0.5f * kCornerLength));

  content.salarymanForward = -frame.contentForward;
  const float clamped_spawn_distance =
      std::max(0.0f, std::min(salaryman_spawn_distance,
                              frame.corridorLength - salaryman_end_margin));
  content.salarymanSpawnPosition =
      frame.contentOrigin + frame.contentForward * clamped_spawn_distance;
  content.salarymanSpawnPosition.y = 0.0f;

  return content;
}

CorridorInstance CreateNewCorridorInstance(int logical_id,
                                           const glm::vec3 &content_forward,
                                           bool has_anomaly) {
  CorridorInstance instance;
  instance.state = MakeCorridorState(logical_id);
  instance.state.has_anomaly = has_anomaly;
  instance.content =
      GenerateCorridorContent(logical_id, content_forward, has_anomaly);
  return instance;
}

std::string PosterOrderString(const CorridorContent &content) {
  std::string poster_order;
  for (const PosterSlotLayout &poster : content.posters) {
    if (!poster_order.empty())
      poster_order += ",";
    poster_order += std::to_string(poster.textureIndex);
  }
  return poster_order;
}

void LogCorridorContentSignature(const char *reason, int render_slot,
                                 int traversal_direction,
                                 const CorridorInstance &corridor_instance,
                                 const glm::vec3 &block_display_offset) {
  if (!kCorridorDebugLogsEnabled)
    return;

  const CorridorContent &content = corridor_instance.content;
  const CorridorContentFrame &frame = content.frame;
  const std::string poster_order = PosterOrderString(content);
  const char *poster_wall_side =
      content.posters.empty() ? "none" : content.posters[0].wallSide;

  printf("Corridor window content: reason=%s, renderSlot=%d, "
         "traversalDirection=%d, currentCorridorId=%d, corridorId=%d, "
         "blockOffset=(%.2f, %.2f, %.2f), contentOrigin=(%.2f, %.2f, %.2f), "
         "contentForward=(%.2f, %.2f, %.2f), contentRight=(%.2f, %.2f, %.2f), "
         "posterOrder=[%s], posterWallSide=%s, npcSpawnPosition=(%.2f, %.2f, "
         "%.2f), npcForward=(%.2f, %.2f, %.2f), lightCount=%d, anomaly=%s\n",
         reason, render_slot, traversal_direction,
         g_CurrentCorridorInstance.state.id, content.corridorId,
         block_display_offset.x, block_display_offset.y, block_display_offset.z,
         frame.contentOrigin.x, frame.contentOrigin.y, frame.contentOrigin.z,
         frame.contentForward.x, frame.contentForward.y, frame.contentForward.z,
         frame.contentRight.x, frame.contentRight.y, frame.contentRight.z,
         poster_order.c_str(), poster_wall_side,
         content.salarymanSpawnPosition.x, content.salarymanSpawnPosition.y,
         content.salarymanSpawnPosition.z, content.salarymanForward.x,
         content.salarymanForward.y, content.salarymanForward.z,
         static_cast<int>(content.lightPositions.size()),
         content.hasAnomaly ? "true" : "false");

  for (const PosterSlotLayout &poster : content.posters) {
    const glm::mat4 poster_basis = Matrix(
        poster.normal.x, poster.up.x, poster.widthAxis.x, poster.position.x,
        poster.normal.y, poster.up.y, poster.widthAxis.y, poster.position.y,
        poster.normal.z, poster.up.z, poster.widthAxis.z, poster.position.z,
        0.0f, 0.0f, 0.0f, 1.0f);

    printf("Corridor window poster: reason=%s, renderSlot=%d, "
           "traversalDirection=%d, corridorId=%d, posterSlot=%d, "
           "posterTextureIndex=%d, posterWallSide=%s, "
           "posterCanonicalPosition=(%.2f, %.2f, %.2f), posterNormal=(%.2f, "
           "%.2f, %.2f), posterTransform=[%.2f %.2f %.2f %.2f | %.2f %.2f %.2f "
           "%.2f | %.2f %.2f %.2f %.2f | %.2f %.2f %.2f %.2f]\n",
           reason, render_slot, traversal_direction, content.corridorId,
           poster.slot, poster.textureIndex, poster.wallSide, poster.position.x,
           poster.position.y, poster.position.z, poster.normal.x,
           poster.normal.y, poster.normal.z, poster_basis[0][0],
           poster_basis[1][0], poster_basis[2][0], poster_basis[3][0],
           poster_basis[0][1], poster_basis[1][1], poster_basis[2][1],
           poster_basis[3][1], poster_basis[0][2], poster_basis[1][2],
           poster_basis[2][2], poster_basis[3][2], poster_basis[0][3],
           poster_basis[1][3], poster_basis[2][3], poster_basis[3][3]);
  }
}

void LogCorridorSlotStability(const char *reason, int traversal_direction,
                              const CorridorInstance &corridor_instance) {
  if (!kCorridorDebugLogsEnabled)
    return;

  const CanonicalCorridorLayout layout = GetCanonicalCorridorLayout();
  const glm::vec2 block_offset = layout.block_offset;
  const glm::vec3 slot_offsets[] = {
      glm::vec3(-block_offset.x, 0.0f, -block_offset.y),
      glm::vec3(0.0f, 0.0f, 0.0f),
      glm::vec3(block_offset.x, 0.0f, block_offset.y)};
  const int render_slots[] = {-1, 0, +1};

  const CorridorContent &content = corridor_instance.content;
  const CorridorContentFrame &frame = content.frame;
  const std::string poster_order = PosterOrderString(content);
  const char *poster_wall_side =
      content.posters.empty() ? "none" : content.posters[0].wallSide;

  for (int i = 0; i < 3; ++i) {
    printf("Corridor slot stability: reason=%s, traversalDirection=%d, "
           "corridorId=%d, renderSlot=%d, blockOffset=(%.2f, %.2f, %.2f), "
           "contentOrigin=(%.2f, %.2f, %.2f), contentForward=(%.2f, %.2f, "
           "%.2f), contentRight=(%.2f, %.2f, %.2f), posterOrder=[%s], "
           "posterWallSide=%s, npcSpawnPosition=(%.2f, %.2f, %.2f), "
           "npcForward=(%.2f, %.2f, %.2f), signatureStable=true\n",
           reason, traversal_direction, content.corridorId, render_slots[i],
           slot_offsets[i].x, slot_offsets[i].y, slot_offsets[i].z,
           frame.contentOrigin.x, frame.contentOrigin.y, frame.contentOrigin.z,
           frame.contentForward.x, frame.contentForward.y,
           frame.contentForward.z, frame.contentRight.x, frame.contentRight.y,
           frame.contentRight.z, poster_order.c_str(), poster_wall_side,
           content.salarymanSpawnPosition.x, content.salarymanSpawnPosition.y,
           content.salarymanSpawnPosition.z, content.salarymanForward.x,
           content.salarymanForward.y, content.salarymanForward.z);
  }
}

void LogCorridorWindow(const char *reason, int traversal_direction) {
  if (!kCorridorDebugLogsEnabled)
    return;

  const CanonicalCorridorLayout layout = GetCanonicalCorridorLayout();
  const glm::vec2 block_offset = layout.block_offset;

  printf("Corridor window update: reason=%s, traversalDirection=%d, "
         "currentCorridorId=%d, activeCorridorIds=[%d,%d,%d], "
         "lastEnteredPhysicalSide=%d, preparedCorridorId=%d, "
         "lastSpawnedCorridorId=%d, salarymanActive=%s\n",
         reason, traversal_direction, g_CurrentCorridorInstance.state.id,
         g_NegativeCandidateCorridorInstance.state.id,
         g_CurrentCorridorInstance.state.id,
         g_PositiveCandidateCorridorInstance.state.id,
         g_LastEnteredPhysicalSide, g_PreparedNextCorridorId,
         g_LastSalarymanSpawnCorridorId,
         g_SalarymanNPC.active ? "true" : "false");

  LogCorridorContentSignature(
      reason, -1, traversal_direction, g_NegativeCandidateCorridorInstance,
      glm::vec3(-block_offset.x, 0.0f, -block_offset.y));
  LogCorridorContentSignature(reason, 0, traversal_direction,
                              g_CurrentCorridorInstance,
                              glm::vec3(0.0f, 0.0f, 0.0f));
  LogCorridorContentSignature(reason, +1, traversal_direction,
                              g_PositiveCandidateCorridorInstance,
                              glm::vec3(block_offset.x, 0.0f, block_offset.y));
  LogCorridorSlotStability(reason, traversal_direction,
                           g_CurrentCorridorInstance);
}

void LogCorridorTransition(const char *reason, int traversal_direction,
                           const CorridorContent &content,
                           const glm::vec3 &player_position) {
  if (!kCorridorDebugLogsEnabled)
    return;

  const CorridorContentFrame &frame = content.frame;
  const std::string poster_order = PosterOrderString(content);
  const char *poster_wall_side =
      content.posters.empty() ? "none" : content.posters[0].wallSide;

  printf("Corridor transition: reason=%s, traversalDirection=%d, "
         "corridorId=%d, contentForward=(%.2f, %.2f, %.2f), "
         "contentRight=(%.2f, %.2f, %.2f), posterOrder=[%s], "
         "posterWallSide=%s, npcSpawnPosition=(%.2f, %.2f, %.2f), "
         "npcForward=(%.2f, %.2f, %.2f), playerPosition=(%.2f, %.2f, %.2f)\n",
         reason, traversal_direction, content.corridorId,
         frame.contentForward.x, frame.contentForward.y, frame.contentForward.z,
         frame.contentRight.x, frame.contentRight.y, frame.contentRight.z,
         poster_order.c_str(), poster_wall_side,
         content.salarymanSpawnPosition.x, content.salarymanSpawnPosition.y,
         content.salarymanSpawnPosition.z, content.salarymanForward.x,
         content.salarymanForward.y, content.salarymanForward.z,
         player_position.x, player_position.y, player_position.z);
}

glm::vec3 ComputeSalarymanSpawnPosition(const CorridorContentFrame &frame) {
  const float salaryman_spawn_distance = frame.corridorLength * 0.70f;
  const float salaryman_end_margin = 6.0f;

  glm::vec3 forward = frame.contentForward;
  if (glm::length(forward) < 0.0001f)
    forward = glm::vec3(0.0f, 0.0f, -1.0f);
  else
    forward = glm::normalize(forward);

  const float clamped_spawn_distance =
      std::max(0.0f, std::min(salaryman_spawn_distance,
                              frame.corridorLength - salaryman_end_margin));
  glm::vec3 spawn_position =
      frame.contentOrigin + forward * clamped_spawn_distance;
  spawn_position.y = 0.0f;
  return spawn_position;
}

const char *PlayerSectionName(int player_section) {
  return (player_section == 1) ? "connector/turn" : "straight";
}

void TrySpawnSalarymanForCorridorContent(const CorridorContent &content,
                                         const glm::vec3 &player_position,
                                         const char *reason) {
  const CorridorContentFrame &frame = content.frame;
  glm::vec3 spawn_forward = content.salarymanForward;
  if (glm::length(spawn_forward) < 0.0001f)
    spawn_forward = frame.contentForward;
  if (glm::length(spawn_forward) < 0.0001f)
    spawn_forward = glm::vec3(0.0f, 0.0f, -1.0f);
  else
    spawn_forward = glm::normalize(spawn_forward);

  const glm::vec3 spawn_position = content.salarymanSpawnPosition;
  const bool already_spawned =
      (g_LastSalarymanSpawnCorridorId == frame.logicalCorridorId);

  (void)reason;
  (void)player_position;
  (void)spawn_position;
  (void)spawn_forward;

  if (already_spawned)
    return;

  SpawnSalarymanForCorridor(g_SalarymanNPC, content, player_position);
  g_LastSalarymanSpawnCorridorId = frame.logicalCorridorId;
}

// Número de texturas carregadas pela função LoadTextureImage()
GLuint g_NumLoadedTextures = 0;

int main(int argc, char *argv[]) {
  // Inicializamos a biblioteca GLFW, utilizada para criar uma janela do
  // sistema operacional, onde poderemos renderizar com OpenGL.

  std::srand(time(NULL));

  int success = glfwInit();
  if (!success) {
    fprintf(stderr, "ERROR: glfwInit() failed.\n");
    std::exit(EXIT_FAILURE);
  }

  // Definimos o callback para impressão de erros da GLFW no terminal
  glfwSetErrorCallback(ErrorCallback);

  // Pedimos para utilizar OpenGL versão 3.3 (ou superior)
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  // Pedimos para utilizar o perfil "core", isto é, utilizaremos somente as
  // funções modernas de OpenGL.
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // Criamos uma janela do sistema operacional, com 800 colunas e 600 linhas
  // de pixels, e com título "INF01047 ...".
  GLFWwindow *window;
  window = glfwCreateWindow(800, 600, "INF01047 - Seu Cartao - Seu Nome", NULL,
                            NULL);
  if (!window) {
    glfwTerminate();
    fprintf(stderr, "ERROR: glfwCreateWindow() failed.\n");
    std::exit(EXIT_FAILURE);
  }

  // Definimos a função de callback que será chamada sempre que o usuário
  // pressionar alguma tecla do teclado ...
  glfwSetKeyCallback(window, KeyCallback);
  // ... ou clicar os botões do mouse ...
  glfwSetMouseButtonCallback(window, MouseButtonCallback);
  // ... ou movimentar o cursor do mouse em cima da janela ...
  glfwSetCursorPosCallback(window, CursorPosCallback);
  // ... ou rolar a "rodinha" do mouse.
  glfwSetScrollCallback(window, ScrollCallback);

  // Indicamos que as chamadas OpenGL deverão renderizar nesta janela
  glfwMakeContextCurrent(window);

  // Carregamento de todas funções definidas por OpenGL 3.3, utilizando a
  // biblioteca GLAD.
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

  // Definimos a função de callback que será chamada sempre que a janela for
  // redimensionada, por consequência alterando o tamanho do "framebuffer"
  // (região de memória onde são armazenados os pixels da imagem).
  glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
  FramebufferSizeCallback(window, 800,
                          600); // Forçamos a chamada do callback acima, para
                                // definir g_ScreenRatio.
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  // Imprimimos no terminal informações sobre a GPU do sistema
  const GLubyte *vendor = glGetString(GL_VENDOR);
  const GLubyte *renderer = glGetString(GL_RENDERER);
  const GLubyte *glversion = glGetString(GL_VERSION);
  const GLubyte *glslversion = glGetString(GL_SHADING_LANGUAGE_VERSION);

  printf("GPU: %s, %s, OpenGL %s, GLSL %s\n", vendor, renderer, glversion,
         glslversion);

  // Carregamos os shaders de vértices e de fragmentos que serão utilizados
  // para renderização. Veja slides 180-200 do documento
  // Aula_03_Rendering_Pipeline_Grafico.pdf.
  //
  LoadShadersFromFiles();

  // Carregamos imagens para serem utilizadas como textura (caminhos relativos
  // a data/)
  LoadTextureImage("wall.png", GL_MIRRORED_REPEAT, GL_MIRRORED_REPEAT);  // 0
  LoadTextureImage("floor.jpg", GL_MIRRORED_REPEAT, GL_MIRRORED_REPEAT); // 1
  LoadTextureImage("ceiling.jpg", GL_MIRRORED_REPEAT,
                   GL_MIRRORED_REPEAT);                                // 2
  LoadTextureImage("poster1.jpg", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE); // 3
  LoadTextureImage("poster2.jpg", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE); // 4
  LoadTextureImage("poster3.jpg", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE); // 5
  LoadTextureImage("poster4.jpg", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE); // 6
  const GLuint kSalarymanTextureUnit = g_NumLoadedTextures;
  CreateSolidColorTexture(178, 168, 150); // 7

  BuildCorridorAndAddToVirtualScene();
  BuildCornerAndAddToVirtualScene();
  BuildPostersAndAddToVirtualScene();
  if (!LoadSalarymanStaticModel(g_SalarymanStaticModel,
                                "assets/salarymanwalking.fbx"))
    std::exit(EXIT_FAILURE);
  g_SalarymanNPC.model = &g_SalarymanStaticModel;
  g_SalarymanNPC.animatedModel = NULL;
  g_SalarymanNPC.animator = NULL;
  if (LoadSalarymanAnimatedModel(g_SalarymanAnimatedModel,
                                 "assets/salarymanwalking.fbx")) {
    g_SalarymanAnimator.model = &g_SalarymanAnimatedModel;
    g_SalarymanAnimator.currentTime = 0.0f;
    UpdateSalarymanAnimation(g_SalarymanAnimator, 0.0f);
    g_SalarymanNPC.animatedModel = &g_SalarymanAnimatedModel;
    g_SalarymanNPC.animator = &g_SalarymanAnimator;
    g_SalarymanNPC.useAnimation = true;
    printf("Salaryman render mode: animated\n");
  } else {
    g_SalarymanNPC.useAnimation = false;
    printf("Salaryman render mode: static fallback\n");
  }

  InitializeCorridorLifecycle();
  {
    const glm::vec3 initial_player_position(
        g_CameraPosition.x, g_CameraPosition.y, g_CameraPosition.z);
    TrySpawnSalarymanForCorridorContent(g_CurrentCorridorInstance.content,
                                        initial_player_position,
                                        "initial_corridor");
  }

  LogCorridorWindow("initial", 0);

  const GLuint kWallTextureUnit = 0;
  const GLuint kFloorTextureUnit = 1;
  const GLuint kCeilingTextureUnit = 2;
  const GLuint kPosterTextureUnits[kPosterCount] = {3, 4, 5, 6};

  Material wall_material;
  wall_material.diffuse_texture_unit = kWallTextureUnit;
  wall_material.specular_strength = 0.9f;
  wall_material.shininess = 96.0f;
  wall_material.ambient_strength = 0.05f;
  wall_material.uv_scale = glm::vec2(1.0f, 1.0f);
  wall_material.uv_offset = glm::vec2(0.0f, 0.0f);

  Material floor_material;
  floor_material.diffuse_texture_unit = kFloorTextureUnit;
  floor_material.specular_strength = 0.35f;
  floor_material.shininess = 48.0f;
  floor_material.ambient_strength = 0.04f;
  floor_material.uv_scale = glm::vec2(1.0f, 1.0f);
  floor_material.uv_offset = glm::vec2(0.0f, 0.0f);

  Material ceiling_material;
  ceiling_material.diffuse_texture_unit = kCeilingTextureUnit;
  ceiling_material.specular_strength = 0.15f;
  ceiling_material.shininess = 20.0f;
  ceiling_material.ambient_strength = 0.03f;
  ceiling_material.uv_scale = glm::vec2(1.0f, 1.0f);
  ceiling_material.uv_offset = glm::vec2(0.0f, 0.0f);

  std::vector<Material> poster_materials;
  poster_materials.reserve(kPosterCount);
  for (int i = 0; i < kPosterCount; ++i) {
    Material poster_material;
    poster_material.diffuse_texture_unit = kPosterTextureUnits[i];
    poster_material.specular_strength = 0.10f;
    poster_material.shininess = 24.0f;
    poster_material.ambient_strength = 0.05f;
    poster_material.uv_scale = glm::vec2(1.0f, 1.0f);
    poster_material.uv_offset = glm::vec2(0.0f, 0.0f);
    poster_materials.push_back(poster_material);
  }

  Material salaryman_material;
  salaryman_material.diffuse_texture_unit = kSalarymanTextureUnit;
  salaryman_material.specular_strength = 0.18f;
  salaryman_material.shininess = 32.0f;
  salaryman_material.ambient_strength = 0.08f;
  salaryman_material.uv_scale = glm::vec2(1.0f, 1.0f);
  salaryman_material.uv_offset = glm::vec2(0.0f, 0.0f);
  const CanonicalCorridorLayout corridor_layout = GetCanonicalCorridorLayout();
  const float connector_length = corridor_layout.connector_length;
  const glm::vec2 block_offset = corridor_layout.block_offset;
  const float turn_z0 = corridor_layout.turn_z0;
  const float connector_center_z = corridor_layout.connector_center_z;
  const float connector_start_x = corridor_layout.connector_start_x;
  const float exit_turn_x = corridor_layout.exit_turn_x;

  std::vector<PointLight> corridor_lights;
  corridor_lights.reserve(kMaxLights);

  auto make_light = [&](const glm::vec3 &position) {
    PointLight light;
    light.position = position;
    light.color = glm::vec3(1.0f, 0.98f, 0.92f);
    light.ambient_strength = 0.03f;
    light.diffuse_strength = 1.0f;
    light.specular_strength = 1.0f;
    light.constant = 1.0f;
    light.linear = 0.14f;
    light.quadratic = 0.07f;
    return light;
  };

  auto make_block_light = [&](const glm::mat4 &block_transform,
                              const glm::vec3 &local_position) {
    glm::vec4 world_position =
        block_transform *
        glm::vec4(local_position.x, local_position.y, local_position.z, 1.0f);
    return make_light(
        glm::vec3(world_position.x, world_position.y, world_position.z));
  };

  // Lambda to generate the lights for one complete modular block.
  auto add_block_lights = [&](const glm::mat4 &block_transform) {
    const float straight_spacing = kCorridorLength / 5.0f;
    for (int i = 0; i < 4; ++i) {
      corridor_lights.push_back(make_block_light(
          block_transform, glm::vec3(0.0f, kCorridorHeight - 0.15f,
                                     -(i + 1) * straight_spacing)));
    }

    corridor_lights.push_back(make_block_light(
        block_transform, glm::vec3(0.0f, kCorridorHeight - 0.15f,
                                   turn_z0 - 0.5f * kCornerLength)));
    corridor_lights.push_back(make_block_light(
        block_transform,
        glm::vec3(connector_start_x - 0.5f * connector_length,
                  kCorridorHeight - 0.15f, connector_center_z)));
    corridor_lights.push_back(make_block_light(
        block_transform, glm::vec3(exit_turn_x, kCorridorHeight - 0.15f,
                                   turn_z0 - 0.5f * kCornerLength)));
  };

  // Apply lights to the three physical treadmill tiles.
  add_block_lights(Matrix_Translate(
      -block_offset.x, 0.0f, -block_offset.y)); // Previous-side candidate
  add_block_lights(Matrix_Identity());          // Current physical block
  add_block_lights(Matrix_Translate(block_offset.x, 0.0f,
                                    block_offset.y)); // Next-side candidate

  // Inicializamos o código para renderização de texto.
  TextRendering_Init();
  // Habilitamos o Z-buffer. Veja slides 104-116 do documento
  // Aula_09_Projecoes.pdf.
  glEnable(GL_DEPTH_TEST);

  // Habilitamos o Backface Culling. Veja slides 8-13 do documento
  // Aula_02_Fundamentos_Matematicos.pdf, slides 23-34 do documento
  // Aula_13_Clipping_and_Culling.pdf e slides 112-123 do documento
  // Aula_14_Laboratorio_3_Revisao.pdf.
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);

  // Ficamos em um loop infinito, renderizando, até que o usuário feche a
  // janela
  float last_frame_time = (float)glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    float current_frame_time = (float)glfwGetTime();
    float delta_time = current_frame_time - last_frame_time;
    last_frame_time = current_frame_time;
    UpdateCameraFromInput(window, delta_time);

    // Aqui executamos as operações de renderização

    // Definimos a cor do "fundo" do framebuffer como branco.  Tal cor é
    // definida como coeficientes RGBA: Red, Green, Blue, Alpha; isto é:
    // Vermelho, Verde, Azul, Alpha (valor de transparência).
    // Conversaremos sobre sistemas de cores nas aulas de Modelos de
    // Iluminação.
    //
    //           R     G     B     A
    glClearColor(0.07f, 0.07f, 0.08f, 1.0f);

    // "Pintamos" todos os pixels do framebuffer com a cor definida acima,
    // e também resetamos todos os pixels do Z-buffer (depth buffer).
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Pedimos para a GPU utilizar o programa de GPU criado acima (contendo
    // os shaders de vértice e fragmentos).
    glUseProgram(g_GpuProgramID);

    glm::vec4 camera_position_c = g_CameraPosition;
    glm::vec4 camera_front_vector = ComputeCameraFrontVector();
    glm::vec4 camera_lookat_l = camera_position_c + camera_front_vector;
    glm::vec4 camera_view_vector = camera_lookat_l - camera_position_c;
    glm::vec4 camera_up_vector = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    UpdateSalarymanNPC(g_SalarymanNPC, delta_time, camera_position_c);

    // Computamos a matriz "View" utilizando os parâmetros da câmera para
    // definir o sistema de coordenadas da câmera.  Veja slides 2-14, 184-190
    // e 236-242 do documento Aula_08_Sistemas_de_Coordenadas.pdf.
    glm::mat4 view = Matrix_Camera_View(camera_position_c, camera_view_vector,
                                        camera_up_vector);

    // Agora computamos a matriz de Projeção.
    glm::mat4 projection;

    // Note que, no sistema de coordenadas da câmera, os planos near e far
    // estão no sentido negativo! Veja slides 176-204 do documento
    // Aula_09_Projecoes.pdf.
    float nearplane = -0.1f; // Posição do "near plane"
    float farplane =
        -(2.0f * kCorridorLength + 10.0f); // Posição do "far plane"

    if (g_UsePerspectiveProjection) {
      // Projeção Perspectiva.
      // Para definição do field of view (FOV), veja slides 205-215 do
      // documento Aula_09_Projecoes.pdf.
      float field_of_view = 3.141592 / 3.0f;
      projection =
          Matrix_Perspective(field_of_view, g_ScreenRatio, nearplane, farplane);
    } else {
      // Projeção Ortográfica.
      // Para definição dos valores l, r, b, t ("left", "right", "bottom",
      // "top"), PARA PROJEÇÃO ORTOGRÁFICA veja slides 219-224 do documento
      // Aula_09_Projecoes.pdf. Projeção ortográfica fixa.
      float t = 1.5f;
      float b = -t;
      float r = t * g_ScreenRatio;
      float l = -r;
      projection = Matrix_Orthographic(l, r, b, t, nearplane, farplane);
    }

    // Enviamos as matrizes "view" e "projection" para a placa de vídeo
    // (GPU). Veja o arquivo "shader_vertex.glsl", onde estas são
    // efetivamente aplicadas em todos os pontos.
    glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE,
                       glm::value_ptr(projection));

    glUniform4f(g_camera_position_uniform, camera_position_c.x,
                camera_position_c.y, camera_position_c.z, camera_position_c.w);
    SetPointLights(corridor_lights);

    auto draw_straight_corridor =
        [&](const CorridorRenderTransform &corridor_render,
            const glm::mat4 &content_placement, bool draw_posters,
            const CorridorInstance &corridor_instance,
            const Material &floor_mat, const Material &ceiling_mat,
            const Material &wall_mat, float segment_start_distance,
            float segment_length_scale) {
          const glm::mat4 &corridor_model = corridor_render.geometryFromLocal;
          glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                             glm::value_ptr(corridor_model));

          Material floor_instance = floor_mat;
          floor_instance.uv_scale.y *= segment_length_scale;
          floor_instance.uv_offset =
              glm::vec2(0.0f, segment_start_distance / kFloorTileSize);
          ApplyMaterial(floor_instance);
          DrawVirtualObject("corridor_floor");

          Material ceiling_instance = ceiling_mat;
          ceiling_instance.uv_scale.y *= segment_length_scale;
          ceiling_instance.uv_offset =
              glm::vec2(0.0f, segment_start_distance / kCeilingTileSize);
          ApplyMaterial(ceiling_instance);
          DrawVirtualObject("corridor_ceiling");

          Material wall_instance = wall_mat;
          wall_instance.uv_scale.x *= segment_length_scale;
          wall_instance.uv_offset =
              glm::vec2(segment_start_distance / kWallTextureTileSize, 0.0f);
          ApplyMaterial(wall_instance);
          DrawVirtualObject("corridor_wall_left");
          DrawVirtualObject("corridor_wall_right");

          if (draw_posters) {
            const CorridorContent &content = corridor_instance.content;

            for (const PosterSlotLayout &poster : content.posters) {
              glm::mat4 poster_basis =
                  Matrix(poster.normal.x, poster.up.x, poster.widthAxis.x,
                         poster.position.x, poster.normal.y, poster.up.y,
                         poster.widthAxis.y, poster.position.y, poster.normal.z,
                         poster.up.z, poster.widthAxis.z, poster.position.z,
                         0.0f, 0.0f, 0.0f, 1.0f);
              glm::mat4 poster_model = content_placement * poster_basis;
              glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                                 glm::value_ptr(poster_model));

              ApplyMaterial(poster_materials[poster.textureIndex]);
              DrawVirtualObject(kPosterNames[poster.slot]);
            }
          }
        };

    Material corner_floor_material = floor_material;
    Material corner_ceiling_material = ceiling_material;
    Material corner_wall_material = wall_material;

    auto make_length_offset_material = [&](const Material &base_material,
                                           float length_offset, bool offset_u,
                                           float tile_size) {
      Material instance = base_material;
      if (offset_u)
        instance.uv_offset.x = length_offset / tile_size;
      else
        instance.uv_offset.y = length_offset / tile_size;
      return instance;
    };

    auto draw_corner = [&](const glm::mat4 &corner_model,
                           const char *floor_name, const char *ceiling_name,
                           const char *wall_a_name, const char *wall_b_name,
                           float length_offset, bool wall_a_uses_length_axis,
                           bool wall_b_uses_length_axis) {
      glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                         glm::value_ptr(corner_model));

      ApplyMaterial(make_length_offset_material(
          corner_floor_material, length_offset, false, kFloorTileSize));
      DrawVirtualObject(floor_name);

      ApplyMaterial(make_length_offset_material(
          corner_ceiling_material, length_offset, false, kCeilingTileSize));
      DrawVirtualObject(ceiling_name);

      ApplyMaterial(make_length_offset_material(
          corner_wall_material, wall_a_uses_length_axis ? length_offset : 0.0f,
          true, kWallTextureTileSize));
      DrawVirtualObject(wall_a_name);

      ApplyMaterial(make_length_offset_material(
          corner_wall_material, wall_b_uses_length_axis ? length_offset : 0.0f,
          true, kWallTextureTileSize));
      DrawVirtualObject(wall_b_name);
    };

    // (1) Modular block (tile): corredor reto + quina esquerda + conector +
    // quina direita. O próximo tile começa em (corridor2_offset_x,
    // second_corridor_z_offset) relativo ao tile atual.
    auto draw_modular_block = [&](const CorridorRenderTransform &block_render,
                                  const glm::mat4 &content_placement,
                                  const CorridorInstance &corridor_instance) {
      const glm::mat4 &base_transform = block_render.geometryFromLocal;
      const float straight_start = 0.0f;
      const float corner1_start = kCorridorLength;
      const float connector1_start = corner1_start + kCornerLength;
      const float corner2_start = connector1_start + connector_length;
      const float full_segment_scale = 1.0f;
      const float connector_segment_scale = connector_length / kCorridorLength;

      // Corredor reto principal (eixo -Z) deste tile.
      draw_straight_corridor(block_render, content_placement, true,
                             corridor_instance, floor_material,
                             ceiling_material, wall_material, straight_start,
                             full_segment_scale);

      // Quina esquerda no final do corredor: base_transform *
      // T(0,0,kCorridorZ1).
      glm::mat4 m = base_transform * Matrix_Translate(0.0f, 0.0f, turn_z0);
      draw_corner(m, "corner_left_floor", "corner_left_ceiling",
                  "corner_left_wall_back", "corner_left_wall_right",
                  corner1_start, false, true);

      // Conector curto (eixo -X): base_transform * T(...) * R_y(+90°) *
      // S(...).
      m = base_transform *
          Matrix_Translate(connector_start_x, 0.0f, connector_center_z) *
          Matrix_Rotate_Y(3.141592f / 2.0f) *
          Matrix_Scale(1.0f, 1.0f, connector_length / kCorridorLength);
      draw_straight_corridor(MakeCorridorRenderTransform(m), content_placement,
                             false, corridor_instance, floor_material,
                             ceiling_material, wall_material, connector1_start,
                             connector_segment_scale);

      // Quina direita no fim do conector: base_transform *
      // T(corridor2_offset_x,0,kCorridorZ1).
      m = base_transform * Matrix_Translate(exit_turn_x, 0.0f, turn_z0);
      draw_corner(m, "corner_right_floor", "corner_right_ceiling",
                  "corner_right_wall_front", "corner_right_wall_left",
                  corner2_start, false, true);
    };

    // (2) 3-Tile Treadmill: side blocks are the canonical previous/next
    // candidates.
    const glm::mat4 negative_slot_transform =
        Matrix_Translate(-block_offset.x, 0.0f, -block_offset.y);
    const glm::mat4 current_slot_transform = Matrix_Identity();
    const glm::mat4 positive_slot_transform =
        Matrix_Translate(block_offset.x, 0.0f, block_offset.y);

    draw_modular_block(MakeCorridorRenderTransform(negative_slot_transform),
                       negative_slot_transform,
                       g_NegativeCandidateCorridorInstance);
    draw_modular_block(MakeCorridorRenderTransform(current_slot_transform),
                       current_slot_transform, g_CurrentCorridorInstance);
    draw_modular_block(MakeCorridorRenderTransform(positive_slot_transform),
                       positive_slot_transform,
                       g_PositiveCandidateCorridorInstance);
    DrawSalarymanNPC(g_SalarymanNPC, salaryman_material);

    // Imprimimos na tela os ângulos de Euler que controlam a rotação do
    // terceiro cubo.
    TextRendering_ShowEulerAngles(window);

    // Imprimimos na informação sobre a matriz de projeção sendo utilizada.
    TextRendering_ShowProjection(window);

    // Imprimimos na tela informação sobre o número de quadros renderizados
    // por segundo (frames per second).
    TextRendering_ShowFramesPerSecond(window);

    // O framebuffer onde OpenGL executa as operações de renderização não
    // é o mesmo que está sendo mostrado para o usuário, caso contrário
    // seria possível ver artefatos conhecidos como "screen tearing". A
    // chamada abaixo faz a troca dos buffers, mostrando para o usuário
    // tudo que foi renderizado pelas funções acima.
    // Veja o link:
    // https://en.wikipedia.org/w/index.php?title=Multiple_buffering&oldid=793452829#Double_buffering_in_computer_graphics
    glfwSwapBuffers(window);

    // Verificamos com o sistema operacional se houve alguma interação do
    // usuário (teclado, mouse, ...). Caso positivo, as funções de callback
    // definidas anteriormente usando glfwSet*Callback() serão chamadas
    // pela biblioteca GLFW.
    glfwPollEvents();
  }

  // Finalizamos o uso dos recursos do sistema operacional
  glfwTerminate();

  // Fim do programa
  return 0;
}

// Função que carrega uma imagem para ser utilizada como textura
void LoadTextureImage(const char *filename, GLint wrap_s, GLint wrap_t) {
  std::string fullpath = std::string("../../data/") + filename;
  printf("Carregando imagem \"%s\"... ", fullpath.c_str());

  // Primeiro fazemos a leitura da imagem do disco
  stbi_set_flip_vertically_on_load(true);
  int width;
  int height;
  int channels;
  unsigned char *data =
      stbi_load(fullpath.c_str(), &width, &height, &channels, 3);

  if (data == NULL) {
    fprintf(stderr, "ERROR: Cannot open image file \"%s\".\n",
            fullpath.c_str());
    std::exit(EXIT_FAILURE);
  }

  printf("OK (%dx%d).\n", width, height);

  // Agora criamos objetos na GPU com OpenGL para armazenar a textura
  GLuint texture_id;
  GLuint sampler_id;
  glGenTextures(1, &texture_id);
  glGenSamplers(1, &sampler_id);

  // Veja slides 95-96 do documento Aula_20_Mapeamento_de_Texturas.pdf
  glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, wrap_s);
  glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, wrap_t);

  // Parâmetros de amostragem da textura.
  glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER,
                      GL_LINEAR_MIPMAP_LINEAR);
  glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Agora enviamos a imagem lida do disco para a GPU
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

  GLuint textureunit = g_NumLoadedTextures;
  glActiveTexture(GL_TEXTURE0 + textureunit);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB,
               GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);
  glBindSampler(textureunit, sampler_id);

  stbi_image_free(data);

  g_NumLoadedTextures += 1;
}

void ApplyMaterial(const Material &material) {
  glUniform1i(g_material_diffuse_uniform, material.diffuse_texture_unit);
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

  // "Ligamos" o VAO. Informamos que queremos utilizar os atributos de
  // vértices apontados pelo VAO criado pela função
  // BuildTrianglesAndAddToVirtualScene(). Veja comentários detalhados dentro
  // da definição de BuildTrianglesAndAddToVirtualScene().
  glBindVertexArray(g_VirtualScene[object_name].vertex_array_object_id);

  // Setamos as variáveis "bbox_min" e "bbox_max" do fragment shader
  // com os parâmetros da axis-aligned bounding box (AABB) do modelo.
  glm::vec3 bbox_min = g_VirtualScene[object_name].bbox_min;
  glm::vec3 bbox_max = g_VirtualScene[object_name].bbox_max;
  glUniform4f(g_bbox_min_uniform, bbox_min.x, bbox_min.y, bbox_min.z, 1.0f);
  glUniform4f(g_bbox_max_uniform, bbox_max.x, bbox_max.y, bbox_max.z, 1.0f);

  // Pedimos para a GPU rasterizar os vértices dos eixos XYZ
  // apontados pelo VAO como linhas. Veja a definição de
  // g_VirtualScene[""] dentro da função BuildTrianglesAndAddToVirtualScene(),
  // e veja a documentação da função glDrawElements() em
  // http://docs.gl/gl3/glDrawElements.
  glDrawElements(
      g_VirtualScene[object_name].rendering_mode,
      g_VirtualScene[object_name].num_indices, GL_UNSIGNED_INT,
      (void *)(g_VirtualScene[object_name].first_index * sizeof(GLuint)));

  // "Desligamos" o VAO, evitando assim que operações posteriores venham a
  // alterar o mesmo. Isso evita bugs.
  glBindVertexArray(0);
}

void CreateSolidColorTexture(unsigned char r, unsigned char g,
                             unsigned char b) {
  GLuint texture_id;
  GLuint sampler_id;
  glGenTextures(1, &texture_id);
  glGenSamplers(1, &sampler_id);

  glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  const unsigned char pixel[3] = {r, g, b};
  const GLuint textureunit = g_NumLoadedTextures;
  glActiveTexture(GL_TEXTURE0 + textureunit);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE,
               pixel);
  glBindSampler(textureunit, sampler_id);
  glBindTexture(GL_TEXTURE_2D, 0);

  g_NumLoadedTextures += 1;
}

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

std::string ResolveExistingPath(const char *filename) {
  const std::string requested(filename);
  const std::string candidates[] = {requested,
                                    std::string("../../") + requested,
                                    std::string("../") + requested};

  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
    std::ifstream file(candidates[i].c_str(), std::ios::binary);
    if (file.good())
      return candidates[i];
  }

  return requested;
}

bool ReadWholeFile(const std::string &path, std::vector<unsigned char> &bytes) {
  std::ifstream file(path.c_str(), std::ios::binary);
  if (!file.good())
    return false;

  file.seekg(0, std::ios::end);
  const std::streamoff size = file.tellg();
  file.seekg(0, std::ios::beg);
  if (size <= 0)
    return false;

  bytes.resize((size_t)size);
  file.read((char *)bytes.data(), size);
  return file.good();
}

bool LoadSalarymanDiffuseTexture(const char *filename, GLuint &texture_id,
                                 GLuint &texture_unit) {
  const std::string fullpath = ResolveExistingPath(filename);
  printf("Loading salaryman diffuse texture \"%s\"... ", fullpath.c_str());

  stbi_set_flip_vertically_on_load(true);
  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char *data =
      stbi_load(fullpath.c_str(), &width, &height, &channels, 3);
  if (data == NULL) {
    fprintf(stderr,
            "FAILED.\nERROR: Cannot open salaryman diffuse texture \"%s\".\n",
            fullpath.c_str());
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
                                      float animation_time) {
  if (node_index < 0 || (size_t)node_index >= model.nodes.size())
    return;

  const SalarymanAnimatedNode &node = model.nodes[(size_t)node_index];
  glm::mat4 node_transform = node.transform;
  const SalarymanAnimationChannel *channel =
      FindAnimationChannel(model.animation, node.name);
  if (channel != NULL)
    node_transform = InterpolateNodeTransform(*channel, animation_time);

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
                                     animation_time);
}

void CalculateSalarymanBoneTransformsAtTime(SalarymanAnimatedModel &model,
                                            float animation_time) {
  if (model.rootNodeIndex < 0)
    return;

  for (size_t i = 0; i < model.finalBoneMatrices.size(); ++i)
    model.finalBoneMatrices[i] = Matrix_Identity();

  CalculateSalarymanBoneTransforms(model, model.rootNodeIndex,
                                   Matrix_Identity(), animation_time);
}
} // namespace

bool LoadSalarymanStaticModel(StaticModel &model, const char *filename) {
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

  printf("OK (%zu vertices estaticos).\n", raw_vertices.size());
  return true;
}

bool LoadSalarymanAnimatedModel(SalarymanAnimatedModel &model,
                                const char *filename) {
  model = SalarymanAnimatedModel();
  const std::string path = ResolveExistingPath(filename);
  printf("Loading animated salaryman FBX \"%s\"...\n", path.c_str());

  Assimp::Importer importer;
  const unsigned int flags =
      aiProcess_Triangulate | aiProcess_GenSmoothNormals |
      aiProcess_LimitBoneWeights | aiProcess_ImproveCacheLocality |
      aiProcess_ValidateDataStructure;

  const aiScene *scene = importer.ReadFile(path, flags);
  if (scene == NULL || scene->mRootNode == NULL) {
    fprintf(stderr, "ERROR: Assimp failed to load animated salaryman: %s\n",
            importer.GetErrorString());
    return false;
  }

  model.meshCount = (int)scene->mNumMeshes;
  model.animationCount = (int)scene->mNumAnimations;
  if (scene->mNumMeshes == 0 || scene->mNumAnimations == 0) {
    fprintf(stderr,
            "ERROR: Animated salaryman requires mesh and animation data. mesh "
            "count=%u animation count=%u\n",
            scene->mNumMeshes, scene->mNumAnimations);
    return false;
  }

  GLuint body_diffuse_texture_id = 0;
  GLuint body_diffuse_texture_unit = 0;
  GLuint hair_diffuse_texture_id = 0;
  GLuint hair_diffuse_texture_unit = 0;
  if (!LoadSalarymanDiffuseTexture("assets/salaryman/Ch33_1001_Diffuse.png",
                                   body_diffuse_texture_id,
                                   body_diffuse_texture_unit))
    return false;
  if (!LoadSalarymanDiffuseTexture("assets/salaryman/Ch33_1002_Diffuse.png",
                                   hair_diffuse_texture_id,
                                   hair_diffuse_texture_unit)) {
    hair_diffuse_texture_id = body_diffuse_texture_id;
    hair_diffuse_texture_unit = body_diffuse_texture_unit;
    printf("Salaryman animated FBX: hair diffuse texture unavailable, using "
           "body diffuse fallback.\n");
  }
  printf("Salaryman animated FBX: body diffuse textureId=%u unit=%u, hair "
         "diffuse textureId=%u unit=%u\n",
         body_diffuse_texture_id, body_diffuse_texture_unit,
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
                  "ERROR: Animated salaryman has more bones than supported "
                  "(%d).\n",
                  kMaxSalarymanBones);
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

    const bool use_hair_texture =
        animated_mesh.name.find("Hair") != std::string::npos ||
        animated_mesh.name.find("hair") != std::string::npos ||
        animated_mesh.materialName.find("Hair") != std::string::npos ||
        animated_mesh.materialName.find("hair") != std::string::npos;
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
        "Salaryman animated mesh texture: mesh=%s material=%s assigned=%s "
        "textureId=%u unit=%u\n",
        animated_mesh.name.empty() ? "<unnamed>" : animated_mesh.name.c_str(),
        animated_mesh.materialName.empty() ? "<unnamed>"
                                           : animated_mesh.materialName.c_str(),
        use_hair_texture ? "Ch33_1002_Diffuse.png" : "Ch33_1001_Diffuse.png",
        animated_mesh.diffuse_texture_id, animated_mesh.diffuse_texture_unit);

    model.meshes.push_back(animated_mesh);
  }

  if (!found_vertex || model.meshes.empty() || model.boneCount == 0) {
    fprintf(stderr,
            "ERROR: Animated salaryman missing usable mesh or bone data. mesh "
            "count=%u bone count=%d animation count=%u\n",
            scene->mNumMeshes, model.boneCount, scene->mNumAnimations);
    return false;
  }

  const aiAnimation *animation = scene->mAnimations[0];
  model.animation.duration = (float)animation->mDuration;
  model.animation.ticksPerSecond = (animation->mTicksPerSecond > 0.0)
                                       ? (float)animation->mTicksPerSecond
                                       : 25.0f;
  if (model.animation.duration <= 0.0f) {
    fprintf(stderr,
            "ERROR: Animated salaryman animation has invalid duration.\n");
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

    StripSalarymanHorizontalRootMotion(channel);

    const int stored_index = (int)model.animation.channels.size();
    model.animation.channelByNodeName[channel.nodeName] = stored_index;
    model.animation.channels.push_back(channel);
  }

  if (model.animation.channels.empty()) {
    fprintf(stderr, "ERROR: Animated salaryman has no animation channels.\n");
    return false;
  }

  const float raw_height = std::max(1.0f, raw_max.y - raw_min.y);
  const float model_scale = 1.75f / raw_height;
  const glm::vec3 origin((raw_min.x + raw_max.x) * 0.5f, raw_min.y,
                         (raw_min.z + raw_max.z) * 0.5f);
  model.normalizationMatrix =
      Matrix_Scale(model_scale, model_scale, model_scale) *
      Matrix_Translate(-origin.x, -origin.y, -origin.z);

  model.finalBoneMatrices.assign(kMaxSalarymanBones, Matrix_Identity());
  CalculateSalarymanBoneTransformsAtTime(model, 0.0f);
  model.loaded = true;

  printf("Salaryman animated FBX: mesh count=%d\n", model.meshCount);
  printf("Salaryman animated FBX: bone count=%d\n", model.boneCount);
  printf("Salaryman animated FBX: animation count=%d\n", model.animationCount);
  printf("Salaryman animated FBX: animation duration=%.3f\n",
         model.animation.duration);
  printf("Salaryman animated FBX: ticks per second=%.3f\n",
         model.animation.ticksPerSecond);
  return true;
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

void SpawnSalarymanForCorridor(SalarymanNPC &salaryman,
                               const CorridorContent &content,
                               const glm::vec3 &player_position) {
  const CorridorContentFrame &frame = content.frame;
  glm::vec3 path_forward = content.salarymanForward;
  if (glm::length(path_forward) < 0.0001f)
    path_forward = frame.contentForward;
  if (glm::length(path_forward) < 0.0001f)
    path_forward = glm::vec3(0.0f, 0.0f, -1.0f);
  else
    path_forward = glm::normalize(path_forward);

  const glm::vec3 spawn_position = content.salarymanSpawnPosition;
  salaryman.active = true;
  salaryman.corridorId = frame.logicalCorridorId;
  salaryman.position = spawn_position;
  salaryman.forward = path_forward;
  salaryman.speed = 1.35f;
  salaryman.corridorLength = frame.corridorLength;
  salaryman.corridorOrigin =
      frame.contentOrigin - path_forward * frame.corridorLength;
  salaryman.useAnimation =
      (salaryman.animatedModel != NULL && salaryman.animatedModel->loaded &&
       salaryman.animator != NULL);
  if (salaryman.useAnimation) {
    salaryman.animator->currentTime = 0.0f;
    UpdateSalarymanAnimation(*salaryman.animator, 0.0f);
  }

  (void)player_position;
}

void UpdateSalarymanNPC(SalarymanNPC &salaryman, float delta_time,
                        const glm::vec4 &camera_position_c) {
  if (!salaryman.active)
    return;

  if (salaryman.useAnimation && salaryman.animator != NULL)
    UpdateSalarymanAnimation(*salaryman.animator, delta_time);

  salaryman.position += salaryman.forward * salaryman.speed * delta_time;

  const float corridor_progress = glm::dot(
      salaryman.position - salaryman.corridorOrigin, salaryman.forward);
  const glm::vec3 camera_position(camera_position_c.x, camera_position_c.y,
                                  camera_position_c.z);
  const float player_distance =
      glm::length(salaryman.position - camera_position);

  if (corridor_progress < -2.0f) {
    if (kCorridorDebugLogsEnabled) {
      printf("Salaryman despawn: reason=behind_spawn corridorId=%d "
             "progress=%.2f "
             "playerDistance=%.2f position=(%.2f, %.2f, %.2f), active=false\n",
             salaryman.corridorId, corridor_progress, player_distance,
             salaryman.position.x, salaryman.position.y, salaryman.position.z);
    }
    salaryman.active = false;
  } else if (corridor_progress > salaryman.corridorLength + 2.0f) {
    if (kCorridorDebugLogsEnabled) {
      printf("Salaryman despawn: reason=exited_corridor corridorId=%d "
             "progress=%.2f playerDistance=%.2f position=(%.2f, %.2f, %.2f), "
             "active=false\n",
             salaryman.corridorId, corridor_progress, player_distance,
             salaryman.position.x, salaryman.position.y, salaryman.position.z);
    }
    salaryman.active = false;
  } else if (player_distance > 55.0f) {
    if (kCorridorDebugLogsEnabled) {
      printf("Salaryman despawn: reason=too_far corridorId=%d progress=%.2f "
             "playerDistance=%.2f position=(%.2f, %.2f, %.2f), active=false\n",
             salaryman.corridorId, corridor_progress, player_distance,
             salaryman.position.x, salaryman.position.y, salaryman.position.z);
    }
    salaryman.active = false;
  }
}

void DrawStaticModel(const StaticModel &model) {
  for (size_t i = 0; i < model.object_names.size(); ++i)
    DrawVirtualObject(model.object_names[i].c_str());
}

void DrawSalarymanNPC(const SalarymanNPC &salaryman, const Material &material) {
  if (!salaryman.active)
    return;

  glm::vec3 forward = salaryman.forward;
  if (glm::length(forward) < 0.0001f)
    forward = glm::vec3(0.0f, 0.0f, -1.0f);
  else
    forward = glm::normalize(forward);

  const glm::vec3 up(0.0f, 1.0f, 0.0f);
  glm::vec3 right = glm::cross(up, forward);
  if (glm::length(right) < 0.0001f)
    right = glm::vec3(1.0f, 0.0f, 0.0f);
  else
    right = glm::normalize(right);

  const glm::vec3 corrected_up = glm::normalize(glm::cross(forward, right));
  const glm::vec3 p = salaryman.position;
  const glm::mat4 model_matrix =
      Matrix(right.x, corrected_up.x, forward.x, p.x, right.y, corrected_up.y,
             forward.y, p.y, right.z, corrected_up.z, forward.z, p.z, 0.0f,
             0.0f, 0.0f, 1.0f);

  ApplyMaterial(material);

  if (salaryman.useAnimation && salaryman.animatedModel != NULL &&
      salaryman.animatedModel->loaded) {
    const glm::mat4 animated_model_matrix =
        model_matrix * salaryman.animatedModel->normalizationMatrix;
    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                       glm::value_ptr(animated_model_matrix));
    DrawAnimatedModel(*salaryman.animatedModel);
  } else if (salaryman.model != NULL) {
    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE,
                       glm::value_ptr(model_matrix));
    DrawStaticModel(*salaryman.model);
  }
}

void BuildPostersAndAddToVirtualScene() {
  struct PosterVertex {
    float px, py, pz, pw;
    float nx, ny, nz, nw;
    float u, v;
  };

  std::vector<PosterVertex> vertices;
  std::vector<GLuint> indices;

  GLuint vertex_array_object_id;
  glGenVertexArrays(1, &vertex_array_object_id);
  glBindVertexArray(vertex_array_object_id);

  auto add_poster_quad = [&](const std::string &name, glm::vec3 p0,
                             glm::vec3 p1, glm::vec3 p2, glm::vec3 p3,
                             glm::vec3 normal) {
    size_t first_index = indices.size();
    GLuint base_vertex = static_cast<GLuint>(vertices.size());

    auto push_vertex = [&](glm::vec3 p, float u, float v) {
      PosterVertex vertex;
      vertex.px = p.x;
      vertex.py = p.y;
      vertex.pz = p.z;
      vertex.pw = 1.0f;
      vertex.nx = normal.x;
      vertex.ny = normal.y;
      vertex.nz = normal.z;
      vertex.nw = 0.0f;
      vertex.u = u;
      vertex.v = v;
      vertices.push_back(vertex);
    };

    // Rotate the poster image 90 degrees counterclockwise on its own axis.
    push_vertex(p0, 1.0f, 0.0f);
    push_vertex(p1, 1.0f, 1.0f);
    push_vertex(p2, 0.0f, 1.0f);
    push_vertex(p3, 0.0f, 0.0f);

    indices.push_back(base_vertex + 0);
    indices.push_back(base_vertex + 1);
    indices.push_back(base_vertex + 2);
    indices.push_back(base_vertex + 0);
    indices.push_back(base_vertex + 2);
    indices.push_back(base_vertex + 3);

    glm::vec3 bbox_min = p0;
    glm::vec3 bbox_max = p0;
    bbox_min.x = std::min(bbox_min.x, p1.x);
    bbox_min.y = std::min(bbox_min.y, p1.y);
    bbox_min.z = std::min(bbox_min.z, p1.z);
    bbox_max.x = std::max(bbox_max.x, p1.x);
    bbox_max.y = std::max(bbox_max.y, p1.y);
    bbox_max.z = std::max(bbox_max.z, p1.z);
    bbox_min.x = std::min(bbox_min.x, p2.x);
    bbox_min.y = std::min(bbox_min.y, p2.y);
    bbox_min.z = std::min(bbox_min.z, p2.z);
    bbox_max.x = std::max(bbox_max.x, p2.x);
    bbox_max.y = std::max(bbox_max.y, p2.y);
    bbox_max.z = std::max(bbox_max.z, p2.z);
    bbox_min.x = std::min(bbox_min.x, p3.x);
    bbox_min.y = std::min(bbox_min.y, p3.y);
    bbox_min.z = std::min(bbox_min.z, p3.z);
    bbox_max.x = std::max(bbox_max.x, p3.x);
    bbox_max.y = std::max(bbox_max.y, p3.y);
    bbox_max.z = std::max(bbox_max.z, p3.z);

    SceneObject object;
    object.name = name;
    object.first_index = first_index;
    object.num_indices = 6;
    object.rendering_mode = GL_TRIANGLES;
    object.vertex_array_object_id = vertex_array_object_id;
    object.bbox_min = bbox_min;
    object.bbox_max = bbox_max;
    g_VirtualScene[name] = object;
  };

  const float poster_width = 1.8f;
  const float poster_height = 1.5f;

  for (int i = 0; i < kPosterCount; ++i) {
    float y0 = -poster_height * 0.5f;
    float y1 = +poster_height * 0.5f;
    float z0 = -poster_width * 0.5f;
    float z1 = +poster_width * 0.5f;

    std::string name = "poster_" + std::to_string(i);
    add_poster_quad(name, glm::vec3(0.0f, y0, z0), glm::vec3(0.0f, y1, z0),
                    glm::vec3(0.0f, y1, z1), glm::vec3(0.0f, y0, z1),
                    glm::vec3(1.0f, 0.0f, 0.0f));
  }

  GLuint vertex_buffer_id;
  glGenBuffers(1, &vertex_buffer_id);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(PosterVertex),
               vertices.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(PosterVertex),
                        (void *)offsetof(PosterVertex, px));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(PosterVertex),
                        (void *)offsetof(PosterVertex, nx));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(PosterVertex),
                        (void *)offsetof(PosterVertex, u));
  glEnableVertexAttribArray(2);

  GLuint index_buffer_id;
  glGenBuffers(1, &index_buffer_id);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_id);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint),
               indices.data(), GL_STATIC_DRAW);

  glBindVertexArray(0);
}

// Função que carrega os shaders de vértices e de fragmentos que serão
// utilizados para renderização. Veja slides 180-200 do documento
// Aula_03_Rendering_Pipeline_Grafico.pdf.
//
void LoadShadersFromFiles() {
  // Note que o caminho para os arquivos "shader_vertex.glsl" e
  // "shader_fragment.glsl" estão fixados, sendo que assumimos a existência
  // da seguinte estrutura no sistema de arquivos:
  //
  //    + FCG_Lab_01/
  //    |
  //    +--+ bin/
  //    |  |
  //    |  +--+ Release/  (ou Debug/ ou Linux/)
  //    |     |
  //    |     o-- main.exe
  //    |
  //    +--+ src/
  //       |
  //       o-- shader_vertex.glsl
  //       |
  //       o-- shader_fragment.glsl
  //
  GLuint vertex_shader_id = LoadShader_Vertex("../../src/shader_vertex.glsl");
  GLuint fragment_shader_id =
      LoadShader_Fragment("../../src/shader_fragment.glsl");

  // Deletamos o programa de GPU anterior, caso ele exista.
  if (g_GpuProgramID != 0)
    glDeleteProgram(g_GpuProgramID);

  // Criamos um programa de GPU utilizando os shaders carregados acima.
  g_GpuProgramID = CreateGpuProgram(vertex_shader_id, fragment_shader_id);

  // Buscamos o endereço das variáveis definidas dentro do Vertex Shader.
  // Utilizaremos estas variáveis para enviar dados para a placa de vídeo
  // (GPU)! Veja arquivo "shader_vertex.glsl" e "shader_fragment.glsl".
  g_model_uniform = glGetUniformLocation(g_GpuProgramID,
                                         "model"); // Variável da matriz "model"
  g_view_uniform = glGetUniformLocation(
      g_GpuProgramID,
      "view"); // Variável da matriz "view" em shader_vertex.glsl
  g_projection_uniform = glGetUniformLocation(
      g_GpuProgramID,
      "projection"); // Variável da matriz "projection" em shader_vertex.glsl
  g_use_skinning_uniform = glGetUniformLocation(g_GpuProgramID, "use_skinning");
  g_bone_matrices_uniform =
      glGetUniformLocation(g_GpuProgramID, "bone_matrices[0]");
  g_bbox_min_uniform = glGetUniformLocation(g_GpuProgramID, "bbox_min");
  g_bbox_max_uniform = glGetUniformLocation(g_GpuProgramID, "bbox_max");
  g_camera_position_uniform =
      glGetUniformLocation(g_GpuProgramID, "camera_position");
  g_material_diffuse_uniform =
      glGetUniformLocation(g_GpuProgramID, "material.diffuse_texture");
  g_material_specular_strength_uniform =
      glGetUniformLocation(g_GpuProgramID, "material.specular_strength");
  g_material_shininess_uniform =
      glGetUniformLocation(g_GpuProgramID, "material.shininess");
  g_material_ambient_strength_uniform =
      glGetUniformLocation(g_GpuProgramID, "material.ambient_strength");
  g_material_uv_scale_uniform =
      glGetUniformLocation(g_GpuProgramID, "material.uv_scale");
  g_material_uv_offset_uniform =
      glGetUniformLocation(g_GpuProgramID, "material.uv_offset");
  g_num_lights_uniform = glGetUniformLocation(g_GpuProgramID, "num_lights");

  for (int i = 0; i < kMaxLights; ++i) {
    std::string prefix = "lights[" + std::to_string(i) + "].";
    g_light_position_uniforms[i] =
        glGetUniformLocation(g_GpuProgramID, (prefix + "position").c_str());
    g_light_color_uniforms[i] =
        glGetUniformLocation(g_GpuProgramID, (prefix + "color").c_str());
    g_light_ambient_strength_uniforms[i] = glGetUniformLocation(
        g_GpuProgramID, (prefix + "ambient_strength").c_str());
    g_light_diffuse_strength_uniforms[i] = glGetUniformLocation(
        g_GpuProgramID, (prefix + "diffuse_strength").c_str());
    g_light_specular_strength_uniforms[i] = glGetUniformLocation(
        g_GpuProgramID, (prefix + "specular_strength").c_str());
    g_light_constant_uniforms[i] =
        glGetUniformLocation(g_GpuProgramID, (prefix + "constant").c_str());
    g_light_linear_uniforms[i] =
        glGetUniformLocation(g_GpuProgramID, (prefix + "linear").c_str());
    g_light_quadratic_uniforms[i] =
        glGetUniformLocation(g_GpuProgramID, (prefix + "quadratic").c_str());
  }
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

// Função que computa as normais de um ObjModel, caso elas não tenham sido
// especificadas dentro do arquivo ".obj"
void ComputeNormals(ObjModel *model) {
  if (!model->attrib.normals.empty())
    return;

  // Primeiro computamos as normais para todos os TRIÂNGULOS.
  // Segundo, computamos as normais dos VÉRTICES através do método proposto
  // por Gouraud, onde a normal de cada vértice vai ser a média das normais de
  // todas as faces que compartilham este vértice e que pertencem ao mesmo
  // "smoothing group".

  // Obtemos a lista dos smoothing groups que existem no objeto
  std::set<unsigned int> sgroup_ids;
  for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
    size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

    assert(model->shapes[shape].mesh.smoothing_group_ids.size() ==
           num_triangles);

    for (size_t triangle = 0; triangle < num_triangles; ++triangle) {
      assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);
      unsigned int sgroup =
          model->shapes[shape].mesh.smoothing_group_ids[triangle];
      assert(sgroup >= 0);
      sgroup_ids.insert(sgroup);
    }
  }

  size_t num_vertices = model->attrib.vertices.size() / 3;
  model->attrib.normals.reserve(3 * num_vertices);

  // Processamos um smoothing group por vez
  for (const unsigned int &sgroup : sgroup_ids) {
    std::vector<int> num_triangles_per_vertex(num_vertices, 0);
    std::vector<glm::vec4> vertex_normals(num_vertices,
                                          glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));

    // Acumulamos as normais dos vértices de todos triângulos deste smoothing
    // group
    for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
      size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

      for (size_t triangle = 0; triangle < num_triangles; ++triangle) {
        unsigned int sgroup_tri =
            model->shapes[shape].mesh.smoothing_group_ids[triangle];

        if (sgroup_tri != sgroup)
          continue;

        glm::vec4 vertices[3];
        for (size_t vertex = 0; vertex < 3; ++vertex) {
          tinyobj::index_t idx =
              model->shapes[shape].mesh.indices[3 * triangle + vertex];
          const float vx = model->attrib.vertices[3 * idx.vertex_index + 0];
          const float vy = model->attrib.vertices[3 * idx.vertex_index + 1];
          const float vz = model->attrib.vertices[3 * idx.vertex_index + 2];
          vertices[vertex] = glm::vec4(vx, vy, vz, 1.0);
        }

        const glm::vec4 a = vertices[0];
        const glm::vec4 b = vertices[1];
        const glm::vec4 c = vertices[2];

        const glm::vec4 n = crossproduct(b - a, c - a);

        for (size_t vertex = 0; vertex < 3; ++vertex) {
          tinyobj::index_t idx =
              model->shapes[shape].mesh.indices[3 * triangle + vertex];
          num_triangles_per_vertex[idx.vertex_index] += 1;
          vertex_normals[idx.vertex_index] += n;
        }
      }
    }

    // Computamos a média das normais acumuladas
    std::vector<size_t> normal_indices(num_vertices, 0);

    for (size_t vertex_index = 0; vertex_index < vertex_normals.size();
         ++vertex_index) {
      if (num_triangles_per_vertex[vertex_index] == 0)
        continue;

      glm::vec4 n = vertex_normals[vertex_index] /
                    (float)num_triangles_per_vertex[vertex_index];
      n /= norm(n);

      model->attrib.normals.push_back(n.x);
      model->attrib.normals.push_back(n.y);
      model->attrib.normals.push_back(n.z);

      size_t normal_index = (model->attrib.normals.size() / 3) - 1;
      normal_indices[vertex_index] = normal_index;
    }

    // Escrevemos os índices das normais para os vértices dos triângulos deste
    // smoothing group
    for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
      size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

      for (size_t triangle = 0; triangle < num_triangles; ++triangle) {
        unsigned int sgroup_tri =
            model->shapes[shape].mesh.smoothing_group_ids[triangle];

        if (sgroup_tri != sgroup)
          continue;

        for (size_t vertex = 0; vertex < 3; ++vertex) {
          tinyobj::index_t idx =
              model->shapes[shape].mesh.indices[3 * triangle + vertex];
          model->shapes[shape]
              .mesh.indices[3 * triangle + vertex]
              .normal_index = normal_indices[idx.vertex_index];
        }
      }
    }
  }
}

void BuildCorridorAndAddToVirtualScene() {
  struct CorridorVertex {
    float px, py, pz, pw;
    float nx, ny, nz, nw;
    float u, v;
  };

  std::vector<CorridorVertex> vertices;
  std::vector<GLuint> indices;

  GLuint vertex_array_object_id;
  glGenVertexArrays(1, &vertex_array_object_id);
  glBindVertexArray(vertex_array_object_id);

  auto add_quad = [&](const std::string &name, glm::vec3 p0, glm::vec3 p1,
                      glm::vec3 p2, glm::vec3 p3, glm::vec3 normal,
                      glm::vec3 uv_origin, glm::vec3 uv_u_axis,
                      glm::vec3 uv_v_axis, float u_tile_size,
                      float v_tile_size) {
    size_t first_index = indices.size();
    GLuint base_vertex = static_cast<GLuint>(vertices.size());

    auto make_uv = [&](const glm::vec3 &p) {
      glm::vec3 u_axis = glm::normalize(uv_u_axis);
      glm::vec3 v_axis = glm::normalize(uv_v_axis);
      glm::vec3 delta = p - uv_origin;
      float u = glm::dot(delta, u_axis);
      float v = glm::dot(delta, v_axis);
      return glm::vec2(u / u_tile_size, v / v_tile_size);
    };

    auto push_vertex = [&](glm::vec3 p, float u, float v) {
      CorridorVertex vertex;
      vertex.px = p.x;
      vertex.py = p.y;
      vertex.pz = p.z;
      vertex.pw = 1.0f;
      vertex.nx = normal.x;
      vertex.ny = normal.y;
      vertex.nz = normal.z;
      vertex.nw = 0.0f;
      vertex.u = u;
      vertex.v = v;
      vertices.push_back(vertex);
    };

    glm::vec2 uv0 = make_uv(p0);
    glm::vec2 uv1 = make_uv(p1);
    glm::vec2 uv2 = make_uv(p2);
    glm::vec2 uv3 = make_uv(p3);

    push_vertex(p0, uv0.x, uv0.y);
    push_vertex(p1, uv1.x, uv1.y);
    push_vertex(p2, uv2.x, uv2.y);
    push_vertex(p3, uv3.x, uv3.y);

    indices.push_back(base_vertex + 0);
    indices.push_back(base_vertex + 1);
    indices.push_back(base_vertex + 2);
    indices.push_back(base_vertex + 0);
    indices.push_back(base_vertex + 2);
    indices.push_back(base_vertex + 3);

    glm::vec3 bbox_min = p0;
    glm::vec3 bbox_max = p0;
    bbox_min.x = std::min(bbox_min.x, p1.x);
    bbox_min.y = std::min(bbox_min.y, p1.y);
    bbox_min.z = std::min(bbox_min.z, p1.z);
    bbox_max.x = std::max(bbox_max.x, p1.x);
    bbox_max.y = std::max(bbox_max.y, p1.y);
    bbox_max.z = std::max(bbox_max.z, p1.z);
    bbox_min.x = std::min(bbox_min.x, p2.x);
    bbox_min.y = std::min(bbox_min.y, p2.y);
    bbox_min.z = std::min(bbox_min.z, p2.z);
    bbox_max.x = std::max(bbox_max.x, p2.x);
    bbox_max.y = std::max(bbox_max.y, p2.y);
    bbox_max.z = std::max(bbox_max.z, p2.z);
    bbox_min.x = std::min(bbox_min.x, p3.x);
    bbox_min.y = std::min(bbox_min.y, p3.y);
    bbox_min.z = std::min(bbox_min.z, p3.z);
    bbox_max.x = std::max(bbox_max.x, p3.x);
    bbox_max.y = std::max(bbox_max.y, p3.y);
    bbox_max.z = std::max(bbox_max.z, p3.z);

    SceneObject object;
    object.name = name;
    object.first_index = first_index;
    object.num_indices = 6;
    object.rendering_mode = GL_TRIANGLES;
    object.vertex_array_object_id = vertex_array_object_id;
    object.bbox_min = bbox_min;
    object.bbox_max = bbox_max;
    g_VirtualScene[name] = object;
  };

  const float half_width = kCorridorHalfWidth;
  const float corridor_height = kCorridorHeight;
  const float corridor_length = kCorridorLength;
  const float z0 = kCorridorZ0;
  const float z1 = kCorridorZ1;

  add_quad("corridor_floor", glm::vec3(-half_width, 0.0f, z0),
           glm::vec3(+half_width, 0.0f, z0), glm::vec3(+half_width, 0.0f, z1),
           glm::vec3(-half_width, 0.0f, z1), glm::vec3(0.0f, 1.0f, 0.0f),
           glm::vec3(-half_width, 0.0f, z0), glm::vec3(1.0f, 0.0f, 0.0f),
           glm::vec3(0.0f, 0.0f, -1.0f), kFloorTileSize, kFloorTileSize);

  add_quad(
      "corridor_ceiling", glm::vec3(-half_width, corridor_height, z1),
      glm::vec3(+half_width, corridor_height, z1),
      glm::vec3(+half_width, corridor_height, z0),
      glm::vec3(-half_width, corridor_height, z0), glm::vec3(0.0f, -1.0f, 0.0f),
      glm::vec3(-half_width, corridor_height, z0), glm::vec3(1.0f, 0.0f, 0.0f),
      glm::vec3(0.0f, 0.0f, -1.0f), kCeilingTileSize, kCeilingTileSize);

  add_quad("corridor_wall_left", glm::vec3(-half_width, 0.0f, z0),
           glm::vec3(-half_width, 0.0f, z1),
           glm::vec3(-half_width, corridor_height, z1),
           glm::vec3(-half_width, corridor_height, z0),
           glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(-half_width, 0.0f, z0),
           glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f),
           kWallTextureTileSize, kWallTextureTileSize);

  add_quad("corridor_wall_right", glm::vec3(+half_width, 0.0f, z1),
           glm::vec3(+half_width, 0.0f, z0),
           glm::vec3(+half_width, corridor_height, z0),
           glm::vec3(+half_width, corridor_height, z1),
           glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(+half_width, 0.0f, z0),
           glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f),
           kWallTextureTileSize, kWallTextureTileSize);

  GLuint vertex_buffer_id;
  glGenBuffers(1, &vertex_buffer_id);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(CorridorVertex),
               vertices.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(CorridorVertex),
                        (void *)offsetof(CorridorVertex, px));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(CorridorVertex),
                        (void *)offsetof(CorridorVertex, nx));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(CorridorVertex),
                        (void *)offsetof(CorridorVertex, u));
  glEnableVertexAttribArray(2);

  GLuint index_buffer_id;
  glGenBuffers(1, &index_buffer_id);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_id);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint),
               indices.data(), GL_STATIC_DRAW);

  glBindVertexArray(0);
}

void BuildCornerAndAddToVirtualScene() {
  struct CornerVertex {
    float px, py, pz, pw;
    float nx, ny, nz, nw;
    float u, v;
  };

  // Agora recebemos 4 booleanos, um para cada parede possível!
  auto build_corner_parts = [&](const std::string &prefix, bool wall_front,
                                bool wall_back, bool wall_left,
                                bool wall_right) {
    auto add_quad = [&](const std::string &name, glm::vec3 p0, glm::vec3 p1,
                        glm::vec3 p2, glm::vec3 p3, glm::vec3 normal,
                        glm::vec3 uv_origin, glm::vec3 uv_u_axis,
                        glm::vec3 uv_v_axis, float u_tile_size,
                        float v_tile_size) {
      std::vector<CornerVertex> vertices;
      std::vector<GLuint> indices = {0, 1, 2, 0, 2, 3};

      auto make_uv = [&](const glm::vec3 &p) {
        glm::vec3 u_axis = glm::normalize(uv_u_axis);
        glm::vec3 v_axis = glm::normalize(uv_v_axis);
        glm::vec3 delta = p - uv_origin;
        float u = glm::dot(delta, u_axis);
        float v = glm::dot(delta, v_axis);
        return glm::vec2(u / u_tile_size, v / v_tile_size);
      };

      glm::vec2 uv0 = make_uv(p0);
      glm::vec2 uv1 = make_uv(p1);
      glm::vec2 uv2 = make_uv(p2);
      glm::vec2 uv3 = make_uv(p3);

      vertices.push_back({p0.x, p0.y, p0.z, 1.0f, normal.x, normal.y, normal.z,
                          0.0f, uv0.x, uv0.y});
      vertices.push_back({p1.x, p1.y, p1.z, 1.0f, normal.x, normal.y, normal.z,
                          0.0f, uv1.x, uv1.y});
      vertices.push_back({p2.x, p2.y, p2.z, 1.0f, normal.x, normal.y, normal.z,
                          0.0f, uv2.x, uv2.y});
      vertices.push_back({p3.x, p3.y, p3.z, 1.0f, normal.x, normal.y, normal.z,
                          0.0f, uv3.x, uv3.y});

      GLuint vao, vbo, ebo;
      glGenVertexArrays(1, &vao);
      glBindVertexArray(vao);
      glGenBuffers(1, &vbo);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(CornerVertex),
                   vertices.data(), GL_STATIC_DRAW);
      glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(CornerVertex),
                            (void *)offsetof(CornerVertex, px));
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(CornerVertex),
                            (void *)offsetof(CornerVertex, nx));
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(CornerVertex),
                            (void *)offsetof(CornerVertex, u));
      glEnableVertexAttribArray(2);
      glGenBuffers(1, &ebo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint),
                   indices.data(), GL_STATIC_DRAW);
      glBindVertexArray(0);

      SceneObject object;
      object.name = name;
      object.first_index = 0;
      object.num_indices = 6;
      object.rendering_mode = GL_TRIANGLES;
      object.vertex_array_object_id = vao;
      object.bbox_min = p3;
      object.bbox_max = p1;
      g_VirtualScene[name] = object;
    };

    const float hw = kCorridorHalfWidth;
    const float h = kCorridorHeight;
    const float z0 = 0.0f;
    const float z1 = -kCornerLength;

    // Chão e Teto
    add_quad(prefix + "_floor", glm::vec3(-hw, 0.0f, z0),
             glm::vec3(hw, 0.0f, z0), glm::vec3(hw, 0.0f, z1),
             glm::vec3(-hw, 0.0f, z1), glm::vec3(0.0f, 1.0f, 0.0f),
             glm::vec3(-hw, 0.0f, z0), glm::vec3(1.0f, 0.0f, 0.0f),
             glm::vec3(0.0f, 0.0f, -1.0f), kFloorTileSize, kFloorTileSize);
    add_quad(prefix + "_ceiling", glm::vec3(-hw, h, z1), glm::vec3(hw, h, z1),
             glm::vec3(hw, h, z0), glm::vec3(-hw, h, z0),
             glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(-hw, h, z0),
             glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
             kCeilingTileSize, kCeilingTileSize);

    // Paredes dinâmicas (só cria se for TRUE)
    if (wall_front)
      add_quad(prefix + "_wall_front", glm::vec3(hw, 0.0f, z0),
               glm::vec3(-hw, 0.0f, z0), glm::vec3(-hw, h, z0),
               glm::vec3(hw, h, z0), glm::vec3(0.0f, 0.0f, -1.0f),
               glm::vec3(hw, 0.0f, z0), glm::vec3(-1.0f, 0.0f, 0.0f),
               glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
               kWallTextureTileSize);
    if (wall_back)
      add_quad(prefix + "_wall_back", glm::vec3(-hw, 0.0f, z1),
               glm::vec3(hw, 0.0f, z1), glm::vec3(hw, h, z1),
               glm::vec3(-hw, h, z1), glm::vec3(0.0f, 0.0f, 1.0f),
               glm::vec3(-hw, 0.0f, z1), glm::vec3(1.0f, 0.0f, 0.0f),
               glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
               kWallTextureTileSize);
    if (wall_left)
      add_quad(prefix + "_wall_left", glm::vec3(-hw, 0.0f, z0),
               glm::vec3(-hw, 0.0f, z1), glm::vec3(-hw, h, z1),
               glm::vec3(-hw, h, z0), glm::vec3(1.0f, 0.0f, 0.0f),
               glm::vec3(-hw, 0.0f, z0), glm::vec3(0.0f, 0.0f, -1.0f),
               glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
               kWallTextureTileSize);
    if (wall_right)
      add_quad(prefix + "_wall_right", glm::vec3(hw, 0.0f, z1),
               glm::vec3(hw, 0.0f, z0), glm::vec3(hw, h, z0),
               glm::vec3(hw, h, z1), glm::vec3(-1.0f, 0.0f, 0.0f),
               glm::vec3(hw, 0.0f, z0), glm::vec3(0.0f, 0.0f, -1.0f),
               glm::vec3(0.0f, 1.0f, 0.0f), kWallTextureTileSize,
               kWallTextureTileSize);
  };

  // Quina 1 (Vira à esquerda): Aberta na frente (para o Corredor 1) e aberta
  // na esquerda (para o Conector). Tem parede no fundo e na direita.
  build_corner_parts("corner_left", false, true, false, true);

  // Quina 2 (Vira à direita): Aberta na direita (vindo do Conector) e aberta
  // no fundo (para o Corredor 2). Tem parede na frente e na esquerda.
  build_corner_parts("corner_right", true, false, true, false);
}

// Constrói triângulos para futura renderização a partir de um ObjModel.
void BuildTrianglesAndAddToVirtualScene(ObjModel *model) {
  GLuint vertex_array_object_id;
  glGenVertexArrays(1, &vertex_array_object_id);
  glBindVertexArray(vertex_array_object_id);

  std::vector<GLuint> indices;
  std::vector<float> model_coefficients;
  std::vector<float> normal_coefficients;
  std::vector<float> texture_coefficients;

  for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
    size_t first_index = indices.size();
    size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

    const float minval = std::numeric_limits<float>::min();
    const float maxval = std::numeric_limits<float>::max();

    glm::vec3 bbox_min = glm::vec3(maxval, maxval, maxval);
    glm::vec3 bbox_max = glm::vec3(minval, minval, minval);

    for (size_t triangle = 0; triangle < num_triangles; ++triangle) {
      assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

      for (size_t vertex = 0; vertex < 3; ++vertex) {
        tinyobj::index_t idx =
            model->shapes[shape].mesh.indices[3 * triangle + vertex];

        indices.push_back(first_index + 3 * triangle + vertex);

        const float vx = model->attrib.vertices[3 * idx.vertex_index + 0];
        const float vy = model->attrib.vertices[3 * idx.vertex_index + 1];
        const float vz = model->attrib.vertices[3 * idx.vertex_index + 2];
        // printf("tri %d vert %d = (%.2f, %.2f, %.2f)\n", (int)triangle,
        // (int)vertex, vx, vy, vz);
        model_coefficients.push_back(vx);   // X
        model_coefficients.push_back(vy);   // Y
        model_coefficients.push_back(vz);   // Z
        model_coefficients.push_back(1.0f); // W

        bbox_min.x = std::min(bbox_min.x, vx);
        bbox_min.y = std::min(bbox_min.y, vy);
        bbox_min.z = std::min(bbox_min.z, vz);
        bbox_max.x = std::max(bbox_max.x, vx);
        bbox_max.y = std::max(bbox_max.y, vy);
        bbox_max.z = std::max(bbox_max.z, vz);

        // Inspecionando o código da tinyobjloader, o aluno Bernardo
        // Sulzbach (2017/1) apontou que a maneira correta de testar se
        // existem normais e coordenadas de textura no ObjModel é
        // comparando se o índice retornado é -1. Fazemos isso abaixo.

        if (idx.normal_index != -1) {
          const float nx = model->attrib.normals[3 * idx.normal_index + 0];
          const float ny = model->attrib.normals[3 * idx.normal_index + 1];
          const float nz = model->attrib.normals[3 * idx.normal_index + 2];
          normal_coefficients.push_back(nx);   // X
          normal_coefficients.push_back(ny);   // Y
          normal_coefficients.push_back(nz);   // Z
          normal_coefficients.push_back(0.0f); // W
        }

        if (idx.texcoord_index != -1) {
          const float u = model->attrib.texcoords[2 * idx.texcoord_index + 0];
          const float v = model->attrib.texcoords[2 * idx.texcoord_index + 1];
          texture_coefficients.push_back(u);
          texture_coefficients.push_back(v);
        }
      }
    }

    size_t last_index = indices.size() - 1;

    SceneObject theobject;
    theobject.name = model->shapes[shape].name;
    theobject.first_index = first_index;                  // Primeiro índice
    theobject.num_indices = last_index - first_index + 1; // Número de indices
    theobject.rendering_mode = GL_TRIANGLES; // Índices correspondem ao tipo
                                             // de rasterização GL_TRIANGLES.
    theobject.vertex_array_object_id = vertex_array_object_id;

    theobject.bbox_min = bbox_min;
    theobject.bbox_max = bbox_max;

    g_VirtualScene[model->shapes[shape].name] = theobject;
  }

  GLuint VBO_model_coefficients_id;
  glGenBuffers(1, &VBO_model_coefficients_id);
  glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
  glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), NULL,
               GL_STATIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, model_coefficients.size() * sizeof(float),
                  model_coefficients.data());
  GLuint location = 0;            // "(location = 0)" em "shader_vertex.glsl"
  GLint number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
  glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0,
                        0);
  glEnableVertexAttribArray(location);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  if (!normal_coefficients.empty()) {
    GLuint VBO_normal_coefficients_id;
    glGenBuffers(1, &VBO_normal_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float),
                 NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    normal_coefficients.size() * sizeof(float),
                    normal_coefficients.data());
    location = 1;             // "(location = 1)" em "shader_vertex.glsl"
    number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0,
                          0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  if (!texture_coefficients.empty()) {
    GLuint VBO_texture_coefficients_id;
    glGenBuffers(1, &VBO_texture_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float),
                 NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    texture_coefficients.size() * sizeof(float),
                    texture_coefficients.data());
    location = 2;             // "(location = 1)" em "shader_vertex.glsl"
    number_of_dimensions = 2; // vec2 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0,
                          0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  GLuint indices_id;
  glGenBuffers(1, &indices_id);

  // "Ligamos" o buffer. Note que o tipo agora é GL_ELEMENT_ARRAY_BUFFER.
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), NULL,
               GL_STATIC_DRAW);
  glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(GLuint),
                  indices.data());
  // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // XXX Errado!
  //

  // "Desligamos" o VAO, evitando assim que operações posteriores venham a
  // alterar o mesmo. Isso evita bugs.
  glBindVertexArray(0);
}

// Carrega um Vertex Shader de um arquivo GLSL. Veja definição de LoadShader()
// abaixo.
GLuint LoadShader_Vertex(const char *filename) {
  // Criamos um identificador (ID) para este shader, informando que o mesmo
  // será aplicado nos vértices.
  GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);

  // Carregamos e compilamos o shader
  LoadShader(filename, vertex_shader_id);

  // Retorna o ID gerado acima
  return vertex_shader_id;
}

// Carrega um Fragment Shader de um arquivo GLSL . Veja definição de
// LoadShader() abaixo.
GLuint LoadShader_Fragment(const char *filename) {
  // Criamos um identificador (ID) para este shader, informando que o mesmo
  // será aplicado nos fragmentos.
  GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

  // Carregamos e compilamos o shader
  LoadShader(filename, fragment_shader_id);

  // Retorna o ID gerado acima
  return fragment_shader_id;
}

// Função auxilar, utilizada pelas duas funções acima. Carrega código de GPU
// de um arquivo GLSL e faz sua compilação.
void LoadShader(const char *filename, GLuint shader_id) {
  // Lemos o arquivo de texto indicado pela variável "filename"
  // e colocamos seu conteúdo em memória, apontado pela variável
  // "shader_string".
  std::ifstream file;
  try {
    file.exceptions(std::ifstream::failbit);
    file.open(filename);
  } catch (std::exception &e) {
    fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", filename);
    std::exit(EXIT_FAILURE);
  }
  std::stringstream shader;
  shader << file.rdbuf();
  std::string str = shader.str();
  const GLchar *shader_string = str.c_str();
  const GLint shader_string_length = static_cast<GLint>(str.length());

  // Define o código do shader GLSL, contido na string "shader_string"
  glShaderSource(shader_id, 1, &shader_string, &shader_string_length);

  // Compila o código do shader GLSL (em tempo de execução)
  glCompileShader(shader_id);

  // Verificamos se ocorreu algum erro ou "warning" durante a compilação
  GLint compiled_ok;
  glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compiled_ok);

  GLint log_length = 0;
  glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

  // Alocamos memória para guardar o log de compilação.
  // A chamada "new" em C++ é equivalente ao "malloc()" do C.
  GLchar *log = new GLchar[log_length];
  glGetShaderInfoLog(shader_id, log_length, &log_length, log);

  // Imprime no terminal qualquer erro ou "warning" de compilação
  if (log_length != 0) {
    std::string output;

    if (!compiled_ok) {
      output += "ERROR: OpenGL compilation of \"";
      output += filename;
      output += "\" failed.\n";
      output += "== Start of compilation log\n";
      output += log;
      output += "== End of compilation log\n";
    } else {
      output += "WARNING: OpenGL compilation of \"";
      output += filename;
      output += "\".\n";
      output += "== Start of compilation log\n";
      output += log;
      output += "== End of compilation log\n";
    }

    fprintf(stderr, "%s", output.c_str());
  }

  // A chamada "delete" em C++ é equivalente ao "free()" do C
  delete[] log;
}

// Esta função cria um programa de GPU, o qual contém obrigatoriamente um
// Vertex Shader e um Fragment Shader.
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id) {
  // Criamos um identificador (ID) para este programa de GPU
  GLuint program_id = glCreateProgram();

  // Definição dos dois shaders GLSL que devem ser executados pelo programa
  glAttachShader(program_id, vertex_shader_id);
  glAttachShader(program_id, fragment_shader_id);

  // Linkagem dos shaders acima ao programa
  glLinkProgram(program_id);

  // Verificamos se ocorreu algum erro durante a linkagem
  GLint linked_ok = GL_FALSE;
  glGetProgramiv(program_id, GL_LINK_STATUS, &linked_ok);

  // Imprime no terminal qualquer erro de linkagem
  if (linked_ok == GL_FALSE) {
    GLint log_length = 0;
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

    // Alocamos memória para guardar o log de compilação.
    // A chamada "new" em C++ é equivalente ao "malloc()" do C.
    GLchar *log = new GLchar[log_length];

    glGetProgramInfoLog(program_id, log_length, &log_length, log);

    std::string output;

    output += "ERROR: OpenGL linking of program failed.\n";
    output += "== Start of link log\n";
    output += log;
    output += "\n== End of link log\n";

    // A chamada "delete" em C++ é equivalente ao "free()" do C
    delete[] log;

    fprintf(stderr, "%s", output.c_str());
  }

  // Os "Shader Objects" podem ser marcados para deleção após serem linkados
  glDeleteShader(vertex_shader_id);
  glDeleteShader(fragment_shader_id);

  // Retornamos o ID gerado acima
  return program_id;
}

glm::vec4 ComputeCameraFrontVector() {
  float cos_pitch = cos(g_CameraPitch);
  glm::vec4 front = glm::vec4(cos_pitch * sin(g_CameraYaw), sin(g_CameraPitch),
                              -cos_pitch * cos(g_CameraYaw), 0.0f);
  return front / norm(front);
}

void UpdateCameraFromInput(GLFWwindow *window, float delta_time) {
  float movement_speed = 10.0f;
  float step = movement_speed * delta_time;

  glm::vec4 front = ComputeCameraFrontVector();
  glm::vec4 world_up = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
  glm::vec4 right = crossproduct(front, world_up);
  right = right / norm(right);

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    g_CameraPosition += front * step;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    g_CameraPosition -= front * step;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    g_CameraPosition -= right * step;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    g_CameraPosition += right * step;

  g_CameraPosition.y = 1.6f;

  const float player_radius = 0.15f;
  const CanonicalCorridorLayout corridor_layout = GetCanonicalCorridorLayout();

  glm::vec2 p_world_raw(g_CameraPosition.x, g_CameraPosition.z);
  CollisionResult col =
      UpdatePlayerCollision(p_world_raw, player_radius, corridor_layout,
                            kCorridorHalfWidth, kCorridorZ1);

  // Repopula as variáveis no escopo para o restante da função não quebrar
  glm::vec2 p_world = col.p_world;
  int block_index = col.block_index;
  bool inside_straight_corridor = col.inside_straight_corridor;
  bool inside_shared_connector = col.inside_shared_connector;
  bool inside_entry_turn = col.inside_entry_turn;
  bool inside_exit_turn = col.inside_exit_turn;
  bool inside_connector_turn = col.inside_connector_turn;
  float connector_progress = col.connector_progress;

  const int player_section = inside_connector_turn ? 1 : 0;
  if (player_section != g_LastPlayerSection) {
    g_LastPlayerSection = player_section;
  }

  auto candidate_for_direction =
      [&](int transition_direction) -> const CorridorInstance & {
    return (transition_direction > 0) ? g_PositiveCandidateCorridorInstance
                                      : g_NegativeCandidateCorridorInstance;
  };

  auto log_connector_enter = [&](int transition_direction) {
    if (transition_direction == 0)
      return;

    g_PreparedTransitionDirection = transition_direction;
    g_PreparedNextCorridorId = -1;
  };

  if (inside_connector_turn) {
    if (!g_InConnectorTransition) {
      g_InConnectorTransition = true;
      g_ConnectorMidpointCrossed = false;
      const int transition_direction = (block_index < 0) ? -1 : +1;
      log_connector_enter(transition_direction);
    }

    const int transition_direction = (g_PreparedTransitionDirection == 0)
                                         ? ((block_index < 0) ? -1 : +1)
                                         : g_PreparedTransitionDirection;
    const bool midpoint_crossed_now = (transition_direction > 0)
                                          ? (connector_progress >= 0.5f)
                                          : (connector_progress <= 0.5f);

    if (!g_ConnectorMidpointCrossed && midpoint_crossed_now) {
      g_PreparedNextCorridorId =
          candidate_for_direction(transition_direction).state.id;
      g_ConnectorMidpointCrossed = true;

      ActivateNewLogicalCorridor(transition_direction);
      if (transition_direction > 0)
        p_world -= corridor_layout.block_offset;
      else
        p_world += corridor_layout.block_offset;

      const glm::vec3 player_position(p_world.x, g_CameraPosition.y, p_world.y);
      TrySpawnSalarymanForCorridorContent(g_CurrentCorridorInstance.content,
                                          player_position,
                                          "connector_midpoint");
      LogCorridorTransition("connector_midpoint", transition_direction,
                            g_CurrentCorridorInstance.content, player_position);
      g_PreparedNextCorridorId = -1;
    }
  } else if (inside_straight_corridor && g_InConnectorTransition) {
    g_InConnectorTransition = false;
    g_PreparedNextCorridorId = -1;
    g_PreparedTransitionDirection = 0;
    g_ConnectorMidpointCrossed = false;
  }

  g_CameraPosition.x = p_world.x;
  g_CameraPosition.z = p_world.y;
  g_CameraPosition.w = 1.0f;
}

// Definição da função que será chamada sempre que a janela do sistema
// operacional for redimensionada, por consequência alterando o tamanho do
// "framebuffer" (região de memória onde são armazenados os pixels da imagem).
void FramebufferSizeCallback(GLFWwindow *window, int width, int height) {
  // Indicamos que queremos renderizar em toda região do framebuffer. A
  // função "glViewport" define o mapeamento das "normalized device
  // coordinates" (NDC) para "pixel coordinates".  Essa é a operação de
  // "Screen Mapping" ou "Viewport Mapping" vista em aula
  // ({+ViewportMapping2+}).
  glViewport(0, 0, width, height);

  // Atualizamos também a razão que define a proporção da janela (largura /
  // altura), a qual será utilizada na definição das matrizes de projeção,
  // tal que não ocorra distorções durante o processo de "Screen Mapping"
  // acima, quando NDC é mapeado para coordenadas de pixels. Veja slides
  // 205-215 do documento Aula_09_Projecoes.pdf.
  //
  // O cast para float é necessário pois números inteiros são arredondados ao
  // serem divididos!
  g_ScreenRatio = (float)width / height;
}

// Variáveis globais que armazenam a última posição do cursor do mouse, para
// que possamos calcular quanto que o mouse se movimentou entre dois instantes
// de tempo. Utilizadas no callback CursorPosCallback() abaixo.
double g_LastCursorPosX, g_LastCursorPosY;

// Função callback chamada sempre que o usuário aperta algum dos botões do
// mouse
void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
    // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
    // posição atual do cursor nas variáveis g_LastCursorPosX e
    // g_LastCursorPosY.  Também, setamos a variável
    // g_LeftMouseButtonPressed como true, para saber que o usuário está
    // com o botão esquerdo pressionado.
    glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
    g_LeftMouseButtonPressed = true;
  }
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
    // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
    // variável abaixo para false.
    g_LeftMouseButtonPressed = false;
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
    // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
    // posição atual do cursor nas variáveis g_LastCursorPosX e
    // g_LastCursorPosY.  Também, setamos a variável
    // g_RightMouseButtonPressed como true, para saber que o usuário está
    // com o botão esquerdo pressionado.
    glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
    g_RightMouseButtonPressed = true;
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
    // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
    // variável abaixo para false.
    g_RightMouseButtonPressed = false;
  }
  if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS) {
    // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
    // posição atual do cursor nas variáveis g_LastCursorPosX e
    // g_LastCursorPosY.  Também, setamos a variável
    // g_MiddleMouseButtonPressed como true, para saber que o usuário está
    // com o botão esquerdo pressionado.
    glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
    g_MiddleMouseButtonPressed = true;
  }
  if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE) {
    // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
    // variável abaixo para false.
    g_MiddleMouseButtonPressed = false;
  }
}

// Função callback chamada sempre que o usuário movimentar o cursor do mouse
// em cima da janela OpenGL.
void CursorPosCallback(GLFWwindow *window, double xpos, double ypos) {
  (void)window;
  if (g_FirstMouseInput) {
    g_LastCursorPosX = xpos;
    g_LastCursorPosY = ypos;
    g_FirstMouseInput = false;
    return;
  }

  float dx = static_cast<float>(xpos - g_LastCursorPosX);
  float dy = static_cast<float>(g_LastCursorPosY - ypos);
  g_LastCursorPosX = xpos;
  g_LastCursorPosY = ypos;

  const float sensitivity = 0.0025f;
  g_CameraYaw += sensitivity * dx;
  g_CameraPitch += sensitivity * dy;

  const float pitch_limit = 1.5533f;
  if (g_CameraPitch > pitch_limit)
    g_CameraPitch = pitch_limit;
  if (g_CameraPitch < -pitch_limit)
    g_CameraPitch = -pitch_limit;
}

// Função callback chamada sempre que o usuário movimenta a "rodinha" do
// mouse.
void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
  (void)window;
  (void)xoffset;
  (void)yoffset;
}

void Correcao_KeyCallback(int key, int action, int mod);

// Definição da função que será chamada sempre que o usuário pressionar alguma
// tecla do teclado. Veja
// http://www.glfw.org/docs/latest/input_guide.html#input_key
void KeyCallback(GLFWwindow *window, int key, int scancode, int action,
                 int mod) {
  // =======================
  // Não modifique esta chamada! Ela é utilizada para correção automatizada
  // dos laboratórios. Deve ser sempre o primeiro comando desta função
  // KeyCallback().
  Correcao_KeyCallback(key, action, mod);
  // =======================

  // Se o usuário pressionar a tecla ESC, fechamos a janela.
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GL_TRUE);

  // O código abaixo implementa a seguinte lógica:
  //   Se apertar tecla X       então g_AngleX += delta;
  //   Se apertar tecla shift+X então g_AngleX -= delta;
  //   Se apertar tecla Y       então g_AngleY += delta;
  //   Se apertar tecla shift+Y então g_AngleY -= delta;
  //   Se apertar tecla Z       então g_AngleZ += delta;
  //   Se apertar tecla shift+Z então g_AngleZ -= delta;

  float delta = 3.141592 / 16; // 22.5 graus, em radianos.

  if (key == GLFW_KEY_X && action == GLFW_PRESS) {
    g_AngleX += (mod & GLFW_MOD_SHIFT) ? -delta : delta;
  }

  if (key == GLFW_KEY_Y && action == GLFW_PRESS) {
    g_AngleY += (mod & GLFW_MOD_SHIFT) ? -delta : delta;
  }
  if (key == GLFW_KEY_Z && action == GLFW_PRESS) {
    g_AngleZ += (mod & GLFW_MOD_SHIFT) ? -delta : delta;
  }

  // Se o usuário apertar a tecla espaço, resetamos os ângulos de Euler para
  // zero.
  if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
    g_AngleX = 0.0f;
    g_AngleY = 0.0f;
    g_AngleZ = 0.0f;
    g_ForearmAngleX = 0.0f;
    g_ForearmAngleZ = 0.0f;
    g_TorsoPositionX = 0.0f;
    g_TorsoPositionY = 0.0f;
  }

  // Se o usuário apertar a tecla P, utilizamos projeção perspectiva.
  if (key == GLFW_KEY_P && action == GLFW_PRESS) {
    g_UsePerspectiveProjection = true;
  }

  // Se o usuário apertar a tecla O, utilizamos projeção ortográfica.
  if (key == GLFW_KEY_O && action == GLFW_PRESS) {
    g_UsePerspectiveProjection = false;
  }

  // Se o usuário apertar a tecla H, fazemos um "toggle" do texto informativo
  // mostrado na tela.
  if (key == GLFW_KEY_H && action == GLFW_PRESS) {
    g_ShowInfoText = !g_ShowInfoText;
  }

  // Se o usuário apertar a tecla R, recarregamos os shaders dos arquivos
  // "shader_fragment.glsl" e "shader_vertex.glsl".
  if (key == GLFW_KEY_R && action == GLFW_PRESS) {
    LoadShadersFromFiles();
    fprintf(stdout, "Shaders recarregados!\n");
    fflush(stdout);
  }
}

// Definimos o callback para impressão de erros da GLFW no terminal
void ErrorCallback(int error, const char *description) {
  fprintf(stderr, "ERROR: GLFW: %s\n", description);
}

// Esta função recebe um vértice com coordenadas de modelo p_model e passa o
// mesmo por todos os sistemas de coordenadas armazenados nas matrizes model,
// view, e projection; e escreve na tela as matrizes e pontos resultantes
// dessas transformações.
void TextRendering_ShowModelViewProjection(GLFWwindow *window,
                                           glm::mat4 projection, glm::mat4 view,
                                           glm::mat4 model, glm::vec4 p_model) {
  if (!g_ShowInfoText)
    return;

  glm::vec4 p_world = model * p_model;
  glm::vec4 p_camera = view * p_world;
  glm::vec4 p_clip = projection * p_camera;
  glm::vec4 p_ndc = p_clip / p_clip.w;

  float pad = TextRendering_LineHeight(window);

  TextRendering_PrintString(
      window, " Model matrix             Model     In World Coords.", -1.0f,
      1.0f - pad, 1.0f);
  TextRendering_PrintMatrixVectorProduct(window, model, p_model, -1.0f,
                                         1.0f - 2 * pad, 1.0f);

  TextRendering_PrintString(window,
                            "                                        |  ",
                            -1.0f, 1.0f - 6 * pad, 1.0f);
  TextRendering_PrintString(window,
                            "                            .-----------'  ",
                            -1.0f, 1.0f - 7 * pad, 1.0f);
  TextRendering_PrintString(window,
                            "                            V              ",
                            -1.0f, 1.0f - 8 * pad, 1.0f);

  TextRendering_PrintString(
      window, " View matrix              World     In Camera Coords.", -1.0f,
      1.0f - 9 * pad, 1.0f);
  TextRendering_PrintMatrixVectorProduct(window, view, p_world, -1.0f,
                                         1.0f - 10 * pad, 1.0f);

  TextRendering_PrintString(window,
                            "                                        |  ",
                            -1.0f, 1.0f - 14 * pad, 1.0f);
  TextRendering_PrintString(window,
                            "                            .-----------'  ",
                            -1.0f, 1.0f - 15 * pad, 1.0f);
  TextRendering_PrintString(window,
                            "                            V              ",
                            -1.0f, 1.0f - 16 * pad, 1.0f);

  TextRendering_PrintString(
      window, " Projection matrix        Camera                    In NDC",
      -1.0f, 1.0f - 17 * pad, 1.0f);
  TextRendering_PrintMatrixVectorProductDivW(window, projection, p_camera,
                                             -1.0f, 1.0f - 18 * pad, 1.0f);

  int width, height;
  glfwGetFramebufferSize(window, &width, &height);

  glm::vec2 a = glm::vec2(-1, -1);
  glm::vec2 b = glm::vec2(+1, +1);
  glm::vec2 p = glm::vec2(0, 0);
  glm::vec2 q = glm::vec2(width, height);

  glm::mat4 viewport_mapping = Matrix(
      (q.x - p.x) / (b.x - a.x), 0.0f, 0.0f,
      (b.x * p.x - a.x * q.x) / (b.x - a.x), 0.0f, (q.y - p.y) / (b.y - a.y),
      0.0f, (b.y * p.y - a.y * q.y) / (b.y - a.y), 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f);

  TextRendering_PrintString(
      window, "                                                       |  ",
      -1.0f, 1.0f - 22 * pad, 1.0f);
  TextRendering_PrintString(
      window, "                            .--------------------------'  ",
      -1.0f, 1.0f - 23 * pad, 1.0f);
  TextRendering_PrintString(
      window, "                            V                           ", -1.0f,
      1.0f - 24 * pad, 1.0f);

  TextRendering_PrintString(
      window, " Viewport matrix           NDC      In Pixel Coords.", -1.0f,
      1.0f - 25 * pad, 1.0f);
  TextRendering_PrintMatrixVectorProductMoreDigits(
      window, viewport_mapping, p_ndc, -1.0f, 1.0f - 26 * pad, 1.0f);
}

// Escrevemos na tela os ângulos de Euler definidos nas variáveis globais
// g_AngleX, g_AngleY, e g_AngleZ.
void TextRendering_ShowEulerAngles(GLFWwindow *window) {
  if (!g_ShowInfoText)
    return;

  float pad = TextRendering_LineHeight(window);

  char buffer[80];
  snprintf(buffer, 80,
           "Euler Angles rotation matrix = Z(%.2f)*Y(%.2f)*X(%.2f)\n", g_AngleZ,
           g_AngleY, g_AngleX);

  TextRendering_PrintString(window, buffer, -1.0f + pad / 10,
                            -1.0f + 2 * pad / 10, 1.0f);
}

// Escrevemos na tela qual matriz de projeção está sendo utilizada.
void TextRendering_ShowProjection(GLFWwindow *window) {
  if (!g_ShowInfoText)
    return;

  float lineheight = TextRendering_LineHeight(window);
  float charwidth = TextRendering_CharWidth(window);

  if (g_UsePerspectiveProjection)
    TextRendering_PrintString(window, "Perspective", 1.0f - 13 * charwidth,
                              -1.0f + 2 * lineheight / 10, 1.0f);
  else
    TextRendering_PrintString(window, "Orthographic", 1.0f - 13 * charwidth,
                              -1.0f + 2 * lineheight / 10, 1.0f);
}

// Escrevemos na tela o número de quadros renderizados por segundo (frames per
// second).
void TextRendering_ShowFramesPerSecond(GLFWwindow *window) {
  if (!g_ShowInfoText)
    return;

  // Variáveis estáticas (static) mantém seus valores entre chamadas
  // subsequentes da função!
  static float old_seconds = (float)glfwGetTime();
  static int ellapsed_frames = 0;
  static char buffer[20] = "?? fps";
  static int numchars = 7;

  ellapsed_frames += 1;

  // Recuperamos o número de segundos que passou desde a execução do programa
  float seconds = (float)glfwGetTime();

  // Número de segundos desde o último cálculo do fps
  float ellapsed_seconds = seconds - old_seconds;

  if (ellapsed_seconds > 1.0f) {
    numchars =
        snprintf(buffer, 20, "%.2f fps", ellapsed_frames / ellapsed_seconds);

    old_seconds = seconds;
    ellapsed_frames = 0;
  }

  float lineheight = TextRendering_LineHeight(window);
  float charwidth = TextRendering_CharWidth(window);

  TextRendering_PrintString(window, buffer, 1.0f - (numchars + 1) * charwidth,
                            1.0f - lineheight, 1.0f);
}

// Função para debugging: imprime no terminal todas informações de um modelo
// geométrico carregado de um arquivo ".obj".
// Veja:
// https://github.com/syoyo/tinyobjloader/blob/22883def8db9ef1f3ffb9b404318e7dd25fdbb51/loader_example.cc#L98
void PrintObjModelInfo(ObjModel *model) {
  const tinyobj::attrib_t &attrib = model->attrib;
  const std::vector<tinyobj::shape_t> &shapes = model->shapes;
  const std::vector<tinyobj::material_t> &materials = model->materials;

  printf("# of vertices  : %d\n", (int)(attrib.vertices.size() / 3));
  printf("# of normals   : %d\n", (int)(attrib.normals.size() / 3));
  printf("# of texcoords : %d\n", (int)(attrib.texcoords.size() / 2));
  printf("# of shapes    : %d\n", (int)shapes.size());
  printf("# of materials : %d\n", (int)materials.size());

  for (size_t v = 0; v < attrib.vertices.size() / 3; v++) {
    printf("  v[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.vertices[3 * v + 0]),
           static_cast<const double>(attrib.vertices[3 * v + 1]),
           static_cast<const double>(attrib.vertices[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.normals.size() / 3; v++) {
    printf("  n[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.normals[3 * v + 0]),
           static_cast<const double>(attrib.normals[3 * v + 1]),
           static_cast<const double>(attrib.normals[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.texcoords.size() / 2; v++) {
    printf("  uv[%ld] = (%f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.texcoords[2 * v + 0]),
           static_cast<const double>(attrib.texcoords[2 * v + 1]));
  }

  // For each shape
  for (size_t i = 0; i < shapes.size(); i++) {
    printf("shape[%ld].name = %s\n", static_cast<long>(i),
           shapes[i].name.c_str());
    printf("Size of shape[%ld].indices: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.indices.size()));

    size_t index_offset = 0;

    assert(shapes[i].mesh.num_face_vertices.size() ==
           shapes[i].mesh.material_ids.size());

    printf("shape[%ld].num_faces: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.num_face_vertices.size()));

    // For each face
    for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++) {
      size_t fnum = shapes[i].mesh.num_face_vertices[f];

      printf("  face[%ld].fnum = %ld\n", static_cast<long>(f),
             static_cast<unsigned long>(fnum));

      // For each vertex in the face
      for (size_t v = 0; v < fnum; v++) {
        tinyobj::index_t idx = shapes[i].mesh.indices[index_offset + v];
        printf("    face[%ld].v[%ld].idx = %d/%d/%d\n", static_cast<long>(f),
               static_cast<long>(v), idx.vertex_index, idx.normal_index,
               idx.texcoord_index);
      }

      printf("  face[%ld].material_id = %d\n", static_cast<long>(f),
             shapes[i].mesh.material_ids[f]);

      index_offset += fnum;
    }

    printf("shape[%ld].num_tags: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.tags.size()));
    for (size_t t = 0; t < shapes[i].mesh.tags.size(); t++) {
      printf("  tag[%ld] = %s ", static_cast<long>(t),
             shapes[i].mesh.tags[t].name.c_str());
      printf(" ints: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].intValues.size(); ++j) {
        printf("%ld", static_cast<long>(shapes[i].mesh.tags[t].intValues[j]));
        if (j < (shapes[i].mesh.tags[t].intValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" floats: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].floatValues.size(); ++j) {
        printf("%f", static_cast<const double>(
                         shapes[i].mesh.tags[t].floatValues[j]));
        if (j < (shapes[i].mesh.tags[t].floatValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" strings: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].stringValues.size(); ++j) {
        printf("%s", shapes[i].mesh.tags[t].stringValues[j].c_str());
        if (j < (shapes[i].mesh.tags[t].stringValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");
      printf("\n");
    }
  }

  for (size_t i = 0; i < materials.size(); i++) {
    printf("material[%ld].name = %s\n", static_cast<long>(i),
           materials[i].name.c_str());
    printf("  material.Ka = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].ambient[0]),
           static_cast<const double>(materials[i].ambient[1]),
           static_cast<const double>(materials[i].ambient[2]));
    printf("  material.Kd = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].diffuse[0]),
           static_cast<const double>(materials[i].diffuse[1]),
           static_cast<const double>(materials[i].diffuse[2]));
    printf("  material.Ks = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].specular[0]),
           static_cast<const double>(materials[i].specular[1]),
           static_cast<const double>(materials[i].specular[2]));
    printf("  material.Tr = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].transmittance[0]),
           static_cast<const double>(materials[i].transmittance[1]),
           static_cast<const double>(materials[i].transmittance[2]));
    printf("  material.Ke = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].emission[0]),
           static_cast<const double>(materials[i].emission[1]),
           static_cast<const double>(materials[i].emission[2]));
    printf("  material.Ns = %f\n",
           static_cast<const double>(materials[i].shininess));
    printf("  material.Ni = %f\n", static_cast<const double>(materials[i].ior));
    printf("  material.dissolve = %f\n",
           static_cast<const double>(materials[i].dissolve));
    printf("  material.illum = %d\n", materials[i].illum);
    printf("  material.map_Ka = %s\n", materials[i].ambient_texname.c_str());
    printf("  material.map_Kd = %s\n", materials[i].diffuse_texname.c_str());
    printf("  material.map_Ks = %s\n", materials[i].specular_texname.c_str());
    printf("  material.map_Ns = %s\n",
           materials[i].specular_highlight_texname.c_str());
    printf("  material.map_bump = %s\n", materials[i].bump_texname.c_str());
    printf("  material.map_d = %s\n", materials[i].alpha_texname.c_str());
    printf("  material.disp = %s\n", materials[i].displacement_texname.c_str());
    printf("  <<PBR>>\n");
    printf("  material.Pr     = %f\n", materials[i].roughness);
    printf("  material.Pm     = %f\n", materials[i].metallic);
    printf("  material.Ps     = %f\n", materials[i].sheen);
    printf("  material.Pc     = %f\n", materials[i].clearcoat_thickness);
    printf("  material.Pcr    = %f\n", materials[i].clearcoat_thickness);
    printf("  material.aniso  = %f\n", materials[i].anisotropy);
    printf("  material.anisor = %f\n", materials[i].anisotropy_rotation);
    printf("  material.map_Ke = %s\n", materials[i].emissive_texname.c_str());
    printf("  material.map_Pr = %s\n", materials[i].roughness_texname.c_str());
    printf("  material.map_Pm = %s\n", materials[i].metallic_texname.c_str());
    printf("  material.map_Ps = %s\n", materials[i].sheen_texname.c_str());
    printf("  material.norm   = %s\n", materials[i].normal_texname.c_str());
    std::map<std::string, std::string>::const_iterator it(
        materials[i].unknown_parameter.begin());
    std::map<std::string, std::string>::const_iterator itEnd(
        materials[i].unknown_parameter.end());

    for (; it != itEnd; it++) {
      printf("  material.%s = %s\n", it->first.c_str(), it->second.c_str());
    }
    printf("\n");
  }
}

// set makeprg=cd\ ..\ &&\ make\ run\ >/dev/null
// vim: set spell spelllang=pt_br :
