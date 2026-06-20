#include "engine/Camera.h"

#include "collisions.h"
#include "engine/Shader.h"
#include "entities/Player.h"
#include "matrices.h"
#include "utils.h"
#include "utils/Constants.h"
#include "world/Corridor.h"
#include "world/TeleportSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

float g_ScreenRatio = 1.0f;
float g_AngleX = 0.0f;
float g_AngleY = 0.0f;
float g_AngleZ = 0.0f;
bool g_LeftMouseButtonPressed = false;
bool g_RightMouseButtonPressed = false;
bool g_MiddleMouseButtonPressed = false;
glm::vec4 g_CameraPosition = glm::vec4(0.0f, 1.6f, -1.0f, 1.0f);
float g_CameraYaw = 0.0f;
float g_CameraPitch = 0.0f;
bool g_FirstMouseInput = true;
float g_ForearmAngleZ = 0.0f;
float g_ForearmAngleX = 0.0f;
float g_TorsoPositionX = 0.0f;
float g_TorsoPositionY = 0.0f;
bool g_UsePerspectiveProjection = true;
bool g_ShowInfoText = true;
bool g_UseThirdPersonCamera = false;
bool g_PlayerInputEnabled = true;
double g_LastCursorPosX = 0.0;
double g_LastCursorPosY = 0.0;

void UpdateThirdPersonCameraFromPlayer();

glm::vec4 ComputeCameraFrontVector() {
  float cos_pitch = cos(g_CameraPitch);
  glm::vec4 front = glm::vec4(cos_pitch * sin(g_CameraYaw), sin(g_CameraPitch),
                              -cos_pitch * cos(g_CameraYaw), 0.0f);
  return front / norm(front);
}

glm::vec4 ComputeCameraViewVector() {
  if (!g_UseThirdPersonCamera)
    return ComputeCameraFrontVector();

  const glm::vec3 target =
      g_PlayerCharacter.position + glm::vec3(0.0f, 1.35f, 0.0f);
  glm::vec3 direction =
      target -
      glm::vec3(g_CameraPosition.x, g_CameraPosition.y, g_CameraPosition.z);
  if (glm::length(direction) < 0.0001f)
    return ComputeCameraFrontVector();

  direction = glm::normalize(direction);
  return glm::vec4(direction.x, direction.y, direction.z, 0.0f);
}

void UpdateThirdPersonCameraFromPlayer() {
  glm::vec4 camera_front_4 = ComputeCameraFrontVector();
  glm::vec3 camera_front(camera_front_4.x, camera_front_4.y, camera_front_4.z);

  const glm::vec3 target =
      g_PlayerCharacter.position + glm::vec3(0.0f, 1.35f, 0.0f);
  glm::vec3 desired_camera = target - camera_front * 4.5f;
  desired_camera.y =
      std::max(0.65f, std::min(kCorridorHeight - 0.20f, desired_camera.y));

  const CanonicalCorridorLayout corridor_layout = GetCanonicalCorridorLayout();
  CollisionResult col = UpdatePlayerCollision(
      glm::vec2(desired_camera.x, desired_camera.z), 0.10f, corridor_layout,
      kCorridorHalfWidth, kCorridorZ1);

  g_CameraPosition =
      glm::vec4(col.p_world.x, desired_camera.y, col.p_world.y, 1.0f);
}

void UpdateCameraFromInput(GLFWwindow *window, float delta_time) {
  if (!g_PlayerInputEnabled)
    return;

  const bool sprint_active =
      glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
  const float sprint_multiplier =
      sprint_active ? kThirdPersonShiftSprintMultiplier : 1.0f;

  if (g_UseThirdPersonCamera) {
    g_PlayerCharacter.locomotionScale = sprint_multiplier;

    glm::vec4 cam_front_4 = ComputeCameraFrontVector();
    glm::vec3 cam_front(cam_front_4.x, 0.0f, cam_front_4.z);
    if (glm::length(cam_front) < 0.0001f)
      cam_front = glm::vec3(0.0f, 0.0f, -1.0f);
    else
      cam_front = glm::normalize(cam_front);

    glm::vec3 cam_right = glm::normalize(glm::cross(cam_front, glm::vec3(0.0f, 1.0f, 0.0f)));

    glm::vec3 movement(0.0f, 0.0f, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      movement += cam_front;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      movement -= cam_front;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      movement -= cam_right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      movement += cam_right;

    const bool input_requests_movement = (glm::length(movement) > 0.0001f);
    const glm::vec3 previous_player_position = g_PlayerCharacter.position;
    if (input_requests_movement) {
      movement = glm::normalize(movement);
      const float movement_speed =
          sprint_active ? GetPlayerThirdPersonShiftSprintSpeed()
                        : g_PlayerCharacter.speed;
      g_PlayerCharacter.position += movement * movement_speed * delta_time;
    }

    const float player_radius = 0.15f;
    const CanonicalCorridorLayout corridor_layout =
        GetCanonicalCorridorLayout();
    CollisionResult col = UpdatePlayerCollision(
        glm::vec2(g_PlayerCharacter.position.x, g_PlayerCharacter.position.z),
        player_radius, corridor_layout, kCorridorHalfWidth, kCorridorZ1);

    glm::vec4 player_collision_position =
        glm::vec4(col.p_world.x, 1.6f, col.p_world.y, 1.0f);
    UpdateTeleportSystem(col, player_collision_position);
    g_PlayerCharacter.position.x = player_collision_position.x;
    g_PlayerCharacter.position.y = 0.0f;
    g_PlayerCharacter.position.z = player_collision_position.z;

    glm::vec3 actual_movement =
        g_PlayerCharacter.position - previous_player_position;
    actual_movement.y = 0.0f;
    g_PlayerCharacter.moving = (glm::length(actual_movement) > 0.0005f);
    if (g_PlayerCharacter.moving) {
      actual_movement = glm::normalize(actual_movement);
      // O personagem agora sempre rotaciona para olhar para a direção do movimento real
      g_PlayerCharacter.forward = actual_movement;
      g_PlayerCharacter.yaw = std::atan2(actual_movement.x, -actual_movement.z);
    }

    UpdatePlayerCharacterAnimation(g_PlayerCharacter, delta_time);
    UpdateThirdPersonCameraFromPlayer();
    return;
  }

  float movement_speed = 10.0f * sprint_multiplier;
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

  UpdateTeleportSystem(col, g_CameraPosition);
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
// Função callback chamada sempre que o usuário aperta algum dos botões do
// mouse
void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
  if (!g_PlayerInputEnabled)
    return;

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
  if (!g_PlayerInputEnabled)
    return;

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

  if (!g_PlayerInputEnabled)
    return;

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

  if (key == GLFW_KEY_C && action == GLFW_PRESS) {
    g_UseThirdPersonCamera = !g_UseThirdPersonCamera;
    if (g_UseThirdPersonCamera) {
      InitializePlayerCharacterFromCamera(g_CameraPosition, g_CameraYaw);
      UpdateThirdPersonCameraFromPlayer();
      fprintf(stdout, "Camera mode: third person\n");
    } else {
      g_CameraPosition = glm::vec4(g_PlayerCharacter.position.x, 1.6f,
                                   g_PlayerCharacter.position.z, 1.0f);
      g_PlayerCharacter.moving = false;
      fprintf(stdout, "Camera mode: first person\n");
    }
    fflush(stdout);
  }

  // Se o usuário apertar a tecla R, recarregamos os shaders dos arquivos
  // "shader_fragment.glsl" e "shader_vertex.glsl".
  if (key == GLFW_KEY_R && action == GLFW_PRESS) {
    LoadShadersFromFiles();
    fprintf(stdout, "Shaders recarregados!\n");
    fflush(stdout);
  }

  // Keyboard shortcuts for testing anomalies
  if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
    ForceNextCorridorAnomaly(kCorridorAnomalyNone);
    printf("Next corridor forced to: Normal (None)\n");
    fflush(stdout);
  }
  if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
    ForceNextCorridorAnomaly(kCorridorAnomalyIdenticalPosters);
    printf("Next corridor forced to: Identical Posters\n");
    fflush(stdout);
  }
  if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
    ForceNextCorridorAnomaly(kCorridorAnomalyNoSmokingSigns);
    printf("Next corridor forced to: No Smoking Signs\n");
    fflush(stdout);
  }
  if (key == GLFW_KEY_4 && action == GLFW_PRESS) {
    ForceNextCorridorAnomaly(kCorridorAnomalyCamouflagedPursuer);
    printf("Next corridor forced to: Camouflaged Pursuer\n");
    fflush(stdout);
  }
  if (key == GLFW_KEY_5 && action == GLFW_PRESS) {
    ForceNextCorridorAnomaly(kCorridorAnomalyGiantNPC);
    printf("Next corridor forced to: Giant NPC\n");
    fflush(stdout);
  }
  if (key == GLFW_KEY_6 && action == GLFW_PRESS) {
    ForceNextCorridorAnomaly(kCorridorAnomalyModifiedFloor);
    printf("Next corridor forced to: Modified Floor\n");
    fflush(stdout);
  }
  if (key == GLFW_KEY_7 && action == GLFW_PRESS) {
    ForceNextCorridorAnomaly(kCorridorAnomalyTwoDoors);
    printf("Next corridor forced to: Two Doors\n");
    fflush(stdout);
  }
  if (key == GLFW_KEY_8 && action == GLFW_PRESS) {
    ForceNextCorridorAnomaly(kCorridorAnomalyScaryPoster);
    printf("Next corridor forced to: Scary Poster\n");
    fflush(stdout);
  }
  if (key == GLFW_KEY_9 && action == GLFW_PRESS) {
    ForceNextCorridorAnomaly(kCorridorAnomalyDoorKnocking);
    printf("Next corridor forced to: Door Knocking\n");
    fflush(stdout);
  }
}

// Definimos o callback para impressão de erros da GLFW no terminal
void ErrorCallback(int error, const char *description) {
  fprintf(stderr, "ERROR: GLFW: %s\n", description);
}
