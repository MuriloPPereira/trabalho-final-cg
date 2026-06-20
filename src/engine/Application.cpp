#include "engine/Application.h"

#include "engine/Audio.h"
#include "engine/Camera.h"
#include "engine/DebugText.h"
#include "engine/Renderer.h"
#include "engine/Shader.h"
#include "engine/Texture.h"
#include "entities/CamouflagedPursuer.h"
#include "entities/NPC.h"
#include "entities/Player.h"
#include "matrices.h"
#include "rendering/WorldRenderer.h"
#include "utils/Constants.h"
#include "world/Corridor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <vector>

namespace {
const float kPursuerDeathDuration = 2.5f;
const float kPursuerDeathFocusHeight = 1.25f;

struct PursuerDeathSequence {
  bool active;
  float elapsed;
  glm::vec4 startCameraPosition;
  glm::vec4 startViewVector;
  glm::vec4 endCameraPosition;
  glm::vec3 focusPosition;

  PursuerDeathSequence()
      : active(false), elapsed(0.0f), startCameraPosition(0.0f),
        startViewVector(0.0f, 0.0f, -1.0f, 0.0f),
        endCameraPosition(0.0f), focusPosition(0.0f) {}
};

float SmoothStep(float t) {
  t = std::max(0.0f, std::min(t, 1.0f));
  return t * t * (3.0f - 2.0f * t);
}

float FootstepVolumeForDistance(const glm::vec3 &source,
                                const glm::vec3 &listener) {
  const float dx = source.x - listener.x;
  const float dz = source.z - listener.z;
  const float distance = std::sqrt(dx * dx + dz * dz);
  const float full_volume_distance = 2.5f;
  const float silent_distance = 28.0f;
  if (distance <= full_volume_distance)
    return 1.0f;
  if (distance >= silent_distance)
    return 0.0f;
  return 1.0f -
         (distance - full_volume_distance) /
             (silent_distance - full_volume_distance);
}

void BeginPursuerDeathSequence(PursuerDeathSequence &death_sequence) {
  death_sequence.active = true;
  death_sequence.elapsed = 0.0f;
  death_sequence.startCameraPosition = g_CameraPosition;
  death_sequence.startViewVector = ComputeCameraViewVector();
  death_sequence.focusPosition =
      g_CamouflagedPursuer.position +
      glm::vec3(0.0f, kPursuerDeathFocusHeight, 0.0f);

  const glm::vec3 start_position(death_sequence.startCameraPosition.x,
                                 death_sequence.startCameraPosition.y,
                                 death_sequence.startCameraPosition.z);
  glm::vec3 focus_to_camera = start_position - death_sequence.focusPosition;
  float start_distance = glm::length(focus_to_camera);
  if (start_distance < 0.0001f) {
    focus_to_camera =
        -glm::vec3(death_sequence.startViewVector.x,
                   death_sequence.startViewVector.y,
                   death_sequence.startViewVector.z);
    if (glm::length(focus_to_camera) < 0.0001f)
      focus_to_camera = glm::vec3(0.0f, 0.0f, 1.0f);
    start_distance = 0.0f;
  }

  focus_to_camera = glm::normalize(focus_to_camera);
  const float end_distance =
      std::max(1.15f, std::min(start_distance + 0.35f, 2.60f));
  glm::vec3 end_position =
      death_sequence.focusPosition + focus_to_camera * end_distance;
  end_position.y = std::max(0.75f, std::min(end_position.y, 2.40f));
  death_sequence.endCameraPosition =
      glm::vec4(end_position.x, end_position.y, end_position.z, 1.0f);

  g_PlayerInputEnabled = false;
  g_LeftMouseButtonPressed = false;
  g_RightMouseButtonPressed = false;
  g_MiddleMouseButtonPressed = false;
  g_PlayerCharacter.moving = false;
}

void ApplyPursuerDeathCamera(const PursuerDeathSequence &death_sequence,
                             glm::vec4 &camera_position,
                             glm::vec4 &camera_view_vector) {
  const float progress = death_sequence.elapsed / kPursuerDeathDuration;
  const float position_blend = SmoothStep(progress / 0.70f);
  const float look_blend = SmoothStep(progress / 0.35f);

  camera_position =
      death_sequence.startCameraPosition * (1.0f - position_blend) +
      death_sequence.endCameraPosition * position_blend;
  camera_position.w = 1.0f;

  glm::vec3 desired_view =
      death_sequence.focusPosition -
      glm::vec3(camera_position.x, camera_position.y, camera_position.z);
  if (glm::length(desired_view) < 0.0001f)
    desired_view = glm::vec3(0.0f, 0.0f, -1.0f);
  else
    desired_view = glm::normalize(desired_view);

  const glm::vec3 start_view(death_sequence.startViewVector.x,
                             death_sequence.startViewVector.y,
                             death_sequence.startViewVector.z);
  glm::vec3 blended_view =
      start_view * (1.0f - look_blend) + desired_view * look_blend;
  if (glm::length(blended_view) < 0.0001f)
    blended_view = desired_view;
  else
    blended_view = glm::normalize(blended_view);
  camera_view_vector =
      glm::vec4(blended_view.x, blended_view.y, blended_view.z, 0.0f);
}

void ResetGameAfterPursuerCatch() {
  const glm::vec4 initial_camera_position(0.0f, 1.6f, -1.0f, 1.0f);

  g_SalarymanNPC.active = false;
  ResetCamouflagedPursuer(g_CamouflagedPursuer);
  InitializeCorridorLifecycle();

  g_CameraPosition = initial_camera_position;
  g_CameraYaw = 0.0f;
  g_CameraPitch = 0.0f;
  g_FirstMouseInput = true;
  g_PlayerInputEnabled = true;
  InitializePlayerCharacterFromCamera(g_CameraPosition, g_CameraYaw);
  g_PlayerCharacter.moving = false;
  g_PlayerCharacter.locomotionScale = 1.0f;
  UpdatePlayerCharacterAnimation(g_PlayerCharacter, 0.0f);
  if (g_UseThirdPersonCamera)
    UpdateThirdPersonCameraFromPlayer();

  const glm::vec3 player_position = g_PlayerCharacter.position;
  TrySpawnSalarymanForCorridorContent(g_CurrentCorridorInstance.content,
                                      player_position, "pursuer_respawn");
  ActivateCamouflagedPursuerForCorridor(
      g_CamouflagedPursuer, g_CurrentCorridorInstance.content);

  printf("\n*** CAUGHT BY CAMOUFLAGED PURSUER - EXIT 8 PROGRESS RESET ***\n\n");
  fflush(stdout);
}
} // namespace

int Application::Run(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

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

  InitializeAudio(NULL);

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
  const GLuint kScaryPosterTextureUnit = g_NumLoadedTextures;
  LoadTextureImage("scary.jpg", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE); // 7
  const GLuint kNoSmokingTextureUnit = g_NumLoadedTextures;
  LoadTextureImage("no_smoking.jpg", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE); // 8
  const GLuint kSalarymanTextureUnit = g_NumLoadedTextures;
  CreateSolidColorTexture(178, 168, 150); // 9
  const GLuint kDoorwayPlaceholderTextureUnit = g_NumLoadedTextures;
  CreateSolidColorTexture(0, 0, 0); // 10
  const GLuint kTactileStraightTextureUnit = g_NumLoadedTextures;
  LoadTextureImage("tactile_straight.jpg", GL_MIRRORED_REPEAT, GL_MIRRORED_REPEAT); // 11
  const GLuint kTactileDotsTextureUnit = g_NumLoadedTextures;
  LoadTextureImage("tactile_dots.jpg", GL_MIRRORED_REPEAT, GL_MIRRORED_REPEAT); // 12
  std::vector<GLuint> exit_sign_texture_units;
  exit_sign_texture_units.reserve(kExitSignCount);
  for (int progress = 0; progress < kExitSignCount; ++progress) {
    exit_sign_texture_units.push_back(g_NumLoadedTextures);
    const std::string filename =
        "exit_sign_" + std::to_string(progress) + "_clean.png";
    LoadTextureImage(filename.c_str(), GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
  }

  BuildCorridorAndAddToVirtualScene();
  BuildCornerAndAddToVirtualScene();
  BuildPostersAndAddToVirtualScene();
  BuildNoSmokingSignAndAddToVirtualScene();
  BuildDoorwayPlaceholderAndAddToVirtualScene();
  if (!LoadSalarymanStaticModel(g_SalarymanStaticModel,
                                "assets/salarymanwalking.fbx"))
    std::exit(EXIT_FAILURE);
  g_SalarymanNPC.model = &g_SalarymanStaticModel;
  g_CamouflagedPursuer.placeholderModel = &g_SalarymanStaticModel;
  g_CamouflagedPursuer.animatedModel = NULL;
  g_CamouflagedPursuer.animator = NULL;
  g_CamouflagedPursuer.useAnimation = false;
  if (LoadTexturedAnimatedModel(
          g_CamouflagedPursuerAnimatedModel, kCamouflagedPursuerFbxPath,
          "data/wall.png", NULL, "camouflaged pursuer")) {
    g_CamouflagedPursuerAnimator.model =
        &g_CamouflagedPursuerAnimatedModel;
    g_CamouflagedPursuerAnimator.currentTime = 0.0f;
    SetAnimatedModelToBindPose(g_CamouflagedPursuerAnimatedModel);
    g_CamouflagedPursuer.animatedModel =
        &g_CamouflagedPursuerAnimatedModel;
    g_CamouflagedPursuer.animator = &g_CamouflagedPursuerAnimator;
    g_CamouflagedPursuer.useAnimation = true;
    printf("Camouflaged pursuer render mode: animated (%s), wall skin\n",
           kCamouflagedPursuerFbxPath);
  } else {
    printf("Camouflaged pursuer render mode: salaryman static fallback\n");
  }
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
  if (!LoadPlayerCharacterModel())
    std::exit(EXIT_FAILURE);
  InitializePlayerCharacterFromCamera(g_CameraPosition, g_CameraYaw);

  InitializeCorridorLifecycle();
  {
    const glm::vec3 initial_player_position(
        g_CameraPosition.x, g_CameraPosition.y, g_CameraPosition.z);
    TrySpawnSalarymanForCorridorContent(g_CurrentCorridorInstance.content,
                                        initial_player_position,
                                        "initial_corridor");
    ActivateCamouflagedPursuerForCorridor(
        g_CamouflagedPursuer, g_CurrentCorridorInstance.content);
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

  Material tactile_straight_material = floor_material;
  tactile_straight_material.diffuse_texture_unit = kTactileStraightTextureUnit;
  tactile_straight_material.uv_scale = glm::vec2(1.0f, 1.0f);

  Material tactile_dots_material = floor_material;
  tactile_dots_material.diffuse_texture_unit = kTactileDotsTextureUnit;
  tactile_dots_material.uv_scale = glm::vec2(1.0f, 1.0f);

  Material ceiling_material;
  ceiling_material.diffuse_texture_unit = kCeilingTextureUnit;
  ceiling_material.specular_strength = 0.15f;
  ceiling_material.shininess = 20.0f;
  ceiling_material.ambient_strength = 0.03f;
  ceiling_material.uv_scale = glm::vec2(1.0f, 1.0f);
  ceiling_material.uv_offset = glm::vec2(0.0f, 0.0f);

  std::vector<Material> poster_materials;
  poster_materials.reserve(kPosterCount + 1);
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
  Material scary_poster_material = poster_materials[0];
  scary_poster_material.diffuse_texture_unit = kScaryPosterTextureUnit;
  poster_materials.push_back(scary_poster_material);

  Material no_smoking_sign_material;
  no_smoking_sign_material.diffuse_texture_unit = kNoSmokingTextureUnit;
  no_smoking_sign_material.specular_strength = 0.12f;
  no_smoking_sign_material.shininess = 18.0f;
  no_smoking_sign_material.ambient_strength = 0.06f;
  no_smoking_sign_material.uv_scale = glm::vec2(1.0f, 1.0f);
  no_smoking_sign_material.uv_offset = glm::vec2(0.0f, 0.0f);

  Material salaryman_material;
  salaryman_material.diffuse_texture_unit = kSalarymanTextureUnit;
  salaryman_material.specular_strength = 0.18f;
  salaryman_material.shininess = 32.0f;
  salaryman_material.ambient_strength = 0.08f;
  salaryman_material.uv_scale = glm::vec2(1.0f, 1.0f);
  salaryman_material.uv_offset = glm::vec2(0.0f, 0.0f);

  Material player_material = salaryman_material;
  player_material.specular_strength = 0.22f;
  player_material.shininess = 36.0f;

  Material camouflaged_pursuer_material = wall_material;
  camouflaged_pursuer_material.specular_strength = 0.12f;
  camouflaged_pursuer_material.shininess = 18.0f;

  Material doorway_placeholder_material;
  doorway_placeholder_material.diffuse_texture_unit =
      kDoorwayPlaceholderTextureUnit;
  doorway_placeholder_material.specular_strength = 0.0f;
  doorway_placeholder_material.shininess = 1.0f;
  doorway_placeholder_material.ambient_strength = 0.0f;
  doorway_placeholder_material.uv_scale = glm::vec2(1.0f, 1.0f);
  doorway_placeholder_material.uv_offset = glm::vec2(0.0f, 0.0f);

  std::vector<Material> exit_sign_materials;
  exit_sign_materials.reserve(kExitSignCount);
  for (int progress = 0; progress < kExitSignCount; ++progress) {
    Material exit_sign_material = poster_materials[0];
    exit_sign_material.diffuse_texture_unit =
        exit_sign_texture_units[progress];
    exit_sign_materials.push_back(exit_sign_material);
  }

  std::vector<PointLight> corridor_lights = CreateCorridorLights();
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
  PursuerDeathSequence pursuer_death;
  LoopingWavSound door_knock_sound("data/audio/door_knock.wav");
  LoopingWavSound walking_steps_sound("data/audio/walking_steps.wav");
  LoopingWavSound running_steps_sound("data/audio/running_steps.wav");
  LoopingWavSound salaryman_steps_sound("data/audio/walking_steps.wav");
  LoopingWavSound pursuer_steps_sound("data/audio/running_steps.wav");
  pursuer_steps_sound.SetGain(1.60f);
  pursuer_steps_sound.SetPlaybackRate(1.15f);
  while (!glfwWindowShouldClose(window)) {
    float current_frame_time = (float)glfwGetTime();
    float delta_time = current_frame_time - last_frame_time;
    last_frame_time = current_frame_time;
    bool death_reset_this_frame = false;
    if (pursuer_death.active) {
      pursuer_death.elapsed += delta_time;
      if (pursuer_death.elapsed >= kPursuerDeathDuration) {
        ResetGameAfterPursuerCatch();
        pursuer_death = PursuerDeathSequence();
        death_reset_this_frame = true;
      }
    }
    const int corridor_id_before_input = g_CurrentCorridorSequenceId;
    const glm::vec3 player_position_before_input =
        g_UseThirdPersonCamera
            ? g_PlayerCharacter.position
            : glm::vec3(g_CameraPosition.x, g_CameraPosition.y,
                        g_CameraPosition.z);
    if (!g_GameWon && !pursuer_death.active && !death_reset_this_frame)
      UpdateCameraFromInput(window, delta_time);
    const glm::vec3 player_position_after_input =
        g_UseThirdPersonCamera
            ? g_PlayerCharacter.position
            : glm::vec3(g_CameraPosition.x, g_CameraPosition.y,
                        g_CameraPosition.z);
    const float player_delta_x =
        player_position_after_input.x - player_position_before_input.x;
    const float player_delta_z =
        player_position_after_input.z - player_position_before_input.z;
    const bool movement_key_down =
        glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
    const bool player_moved_this_frame =
        movement_key_down &&
        player_delta_x * player_delta_x + player_delta_z * player_delta_z >
            0.00000001f;
    const bool sprint_key_down =
        glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    const bool teleported_this_frame =
        corridor_id_before_input != g_CurrentCorridorSequenceId;

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
    const glm::vec3 salaryman_position_before_update =
        g_SalarymanNPC.position;
    if (!g_GameWon && !pursuer_death.active && !death_reset_this_frame)
      UpdateSalarymanNPC(g_SalarymanNPC, delta_time, camera_position_c);
    const glm::vec3 salaryman_delta =
        g_SalarymanNPC.position - salaryman_position_before_update;
    const bool salaryman_moved_this_frame =
        g_SalarymanNPC.active &&
        salaryman_delta.x * salaryman_delta.x +
                salaryman_delta.z * salaryman_delta.z >
            0.00000001f;
    const glm::vec3 pursuer_target =
        g_UseThirdPersonCamera
            ? g_PlayerCharacter.position
            : glm::vec3(camera_position_c.x, camera_position_c.y,
                        camera_position_c.z);
    const glm::vec3 pursuer_position_before_update =
        g_CamouflagedPursuer.position;
    if (!g_GameWon && !pursuer_death.active && !death_reset_this_frame)
      UpdateCamouflagedPursuer(g_CamouflagedPursuer, delta_time,
                               pursuer_target);
    const glm::vec3 pursuer_delta =
        g_CamouflagedPursuer.position - pursuer_position_before_update;
    const bool pursuer_moved_this_frame =
        g_CamouflagedPursuer.active && g_CamouflagedPursuer.visible &&
        g_CamouflagedPursuer.chasing &&
        pursuer_delta.x * pursuer_delta.x +
                pursuer_delta.z * pursuer_delta.z >
            0.00000001f;

    if (!g_GameWon && !pursuer_death.active &&
        HasCamouflagedPursuerCaughtPlayer(
                          g_CamouflagedPursuer, pursuer_target)) {
      BeginPursuerDeathSequence(pursuer_death);
    }

    const bool door_knock_should_play =
        !g_GameWon && !pursuer_death.active &&
        g_LastPlayerSection == 0 &&
        g_CurrentCorridorInstance.state.has_anomaly &&
        g_CurrentCorridorInstance.state.anomaly_type ==
            kCorridorAnomalyDoorKnocking;
    door_knock_sound.SetPlaying(door_knock_should_play);

    const bool footstep_audio_enabled =
        !g_GameWon && !pursuer_death.active && !death_reset_this_frame &&
        g_PlayerInputEnabled && player_moved_this_frame;
    if (footstep_audio_enabled && sprint_key_down) {
      walking_steps_sound.Stop();
      running_steps_sound.SetPlaying(true);
    } else if (footstep_audio_enabled) {
      running_steps_sound.Stop();
      walking_steps_sound.SetPlaying(true);
    } else {
      walking_steps_sound.Stop();
      running_steps_sound.Stop();
    }

    const bool entity_audio_enabled =
        !g_GameWon && !pursuer_death.active && !death_reset_this_frame &&
        !teleported_this_frame;
    const float salaryman_volume =
        FootstepVolumeForDistance(g_SalarymanNPC.position, pursuer_target);
    salaryman_steps_sound.SetVolume(salaryman_volume);
    salaryman_steps_sound.SetPlaying(
        entity_audio_enabled && salaryman_moved_this_frame &&
        salaryman_volume > 0.0f);

    const float pursuer_volume = FootstepVolumeForDistance(
        g_CamouflagedPursuer.position, pursuer_target);
    pursuer_steps_sound.SetVolume(pursuer_volume);
    pursuer_steps_sound.SetPlaying(
        entity_audio_enabled && pursuer_moved_this_frame &&
        pursuer_volume > 0.0f);

    camera_position_c = g_CameraPosition;
    glm::vec4 camera_view_vector = ComputeCameraViewVector();
    if (pursuer_death.active)
      ApplyPursuerDeathCamera(pursuer_death, camera_position_c,
                              camera_view_vector);
    glm::vec4 camera_up_vector = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);

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
      float field_of_view = 3.141592f / 3.0f;
      if (pursuer_death.active) {
        const float death_progress =
            pursuer_death.elapsed / kPursuerDeathDuration;
        field_of_view -= (3.141592f / 22.5f) * SmoothStep(death_progress);
      }
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

    DrawCorridorTreadmill(floor_material, tactile_straight_material, tactile_dots_material, ceiling_material, wall_material,
                           poster_materials, no_smoking_sign_material,
                           doorway_placeholder_material, exit_sign_materials);
    DrawSalarymanNPC(g_SalarymanNPC, salaryman_material);
    DrawCamouflagedPursuer(g_CamouflagedPursuer,
                           camouflaged_pursuer_material);
    if (g_UseThirdPersonCamera)
      DrawPlayerCharacter(g_PlayerCharacter, player_material);

    // Imprimimos na tela informação sobre o número de quadros renderizados
    // por segundo (frames per second).
    TextRendering_ShowFramesPerSecond(window);

    if (g_ShowInfoText) {
      TextRendering_PrintString(window, "Shortcuts:", -0.98f, -0.60f, 1.0f);
      TextRendering_PrintString(window, "1: Normal", -0.98f, -0.65f, 1.0f);
      TextRendering_PrintString(window, "2: Identical Posters", -0.98f, -0.70f, 1.0f);
      TextRendering_PrintString(window, "3: No Smoking Signs", -0.98f, -0.75f, 1.0f);
      TextRendering_PrintString(window, "4: Camouflaged Pursuer", -0.98f, -0.80f, 1.0f);
      TextRendering_PrintString(window, "5: Giant NPC", -0.98f, -0.85f, 1.0f);
      TextRendering_PrintString(window, "6: Modified Floor", -0.98f, -0.90f, 1.0f);
      TextRendering_PrintString(window, "7: Two Doors | 8: Scary Poster | 9: Door Knock", -0.98f, -0.95f, 1.0f);
    }

    if (pursuer_death.active) {
      const std::string death_message = "YOU DIED";
      const std::string caught_message = "CAUGHT";
      const float death_scale = 2.4f;
      const float caught_scale = 1.2f;
      const float char_width = TextRendering_CharWidth(window);
      const float death_x =
          -0.5f * static_cast<float>(death_message.size()) * char_width *
          death_scale;
      const float caught_x =
          -0.5f * static_cast<float>(caught_message.size()) * char_width *
          caught_scale;
      TextRendering_PrintString(window, death_message, death_x, 0.08f,
                                death_scale);
      TextRendering_PrintString(window, caught_message, caught_x, -0.08f,
                                caught_scale);
    }

    if (g_GameWon) {
      const std::string win_message = "YOU WIN - EXIT 8 REACHED";
      const std::string exit_message = "PRESS ESC TO EXIT";
      const float win_scale = 2.0f;
      const float exit_scale = 1.0f;
      const float char_width = TextRendering_CharWidth(window);
      const float win_x =
          -0.5f * static_cast<float>(win_message.size()) * char_width *
          win_scale;
      const float exit_x =
          -0.5f * static_cast<float>(exit_message.size()) * char_width *
          exit_scale;

      TextRendering_PrintString(window, win_message, win_x, 0.10f, win_scale);
      TextRendering_PrintString(window, exit_message, exit_x, -0.05f,
                                exit_scale);
    }

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
  salaryman_steps_sound.Stop();
  pursuer_steps_sound.Stop();
  walking_steps_sound.Stop();
  running_steps_sound.Stop();
  door_knock_sound.Stop();
  ShutdownAudio();
  glfwTerminate();

  // Fim do programa
  return 0;
}
