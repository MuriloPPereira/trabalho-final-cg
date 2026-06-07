#include "engine/Application.h"

#include "engine/Camera.h"
#include "engine/DebugText.h"
#include "engine/Renderer.h"
#include "engine/Shader.h"
#include "engine/Texture.h"
#include "entities/NPC.h"
#include "entities/Player.h"
#include "matrices.h"
#include "rendering/WorldRenderer.h"
#include "utils/Constants.h"
#include "world/Corridor.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

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
  const GLuint kDoorwayPlaceholderTextureUnit = g_NumLoadedTextures;
  CreateSolidColorTexture(0, 0, 0); // 8

  BuildCorridorAndAddToVirtualScene();
  BuildCornerAndAddToVirtualScene();
  BuildPostersAndAddToVirtualScene();
  BuildDoorwayPlaceholderAndAddToVirtualScene();
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

  Material player_material = salaryman_material;
  player_material.specular_strength = 0.22f;
  player_material.shininess = 36.0f;

  Material doorway_placeholder_material;
  doorway_placeholder_material.diffuse_texture_unit =
      kDoorwayPlaceholderTextureUnit;
  doorway_placeholder_material.specular_strength = 0.0f;
  doorway_placeholder_material.shininess = 1.0f;
  doorway_placeholder_material.ambient_strength = 0.0f;
  doorway_placeholder_material.uv_scale = glm::vec2(1.0f, 1.0f);
  doorway_placeholder_material.uv_offset = glm::vec2(0.0f, 0.0f);

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
    glm::vec4 camera_front_vector = ComputeCameraViewVector();
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

    DrawCorridorTreadmill(floor_material, ceiling_material, wall_material,
                          poster_materials, doorway_placeholder_material);
    DrawSalarymanNPC(g_SalarymanNPC, salaryman_material);
    if (g_UseThirdPersonCamera)
      DrawPlayerCharacter(g_PlayerCharacter, player_material);

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
