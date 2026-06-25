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

namespace {
constexpr float kThirdPersonBoomDistance = 4.5f;
constexpr float kThirdPersonCameraRadius = 0.14f;
constexpr float kThirdPersonCameraBackoff = 0.04f;
constexpr float kThirdPersonPlayerHideDistance = 1.05f;
constexpr int kThirdPersonCameraSweepSteps = 48;

float ClampFloat(float value, float min_value, float max_value) {
  return std::max(min_value, std::min(max_value, value));
}

bool IsInsideWalkableBox(const WalkableBox2D &box, const glm::vec2 &p) {
  return p.x >= box.min_x && p.x <= box.max_x && p.y >= box.min_z &&
         p.y <= box.max_z;
}

glm::vec2 WrapCameraPointToLocal(const glm::vec2 &world_point,
                                 const CanonicalCorridorLayout &layout,
                                 float radius, int &block_index) {
  glm::vec2 p = world_point;
  block_index = 0;

  const float first_z_max = 0.8f;
  const float block_length = glm::length(layout.block_offset);
  const glm::vec2 block_dir = layout.block_offset / block_length;
  const float wrap_forward_progress =
      glm::dot(layout.block_offset + glm::vec2(0.0f, radius), block_dir);
  const float wrap_backward_progress =
      glm::dot(glm::vec2(0.0f, first_z_max), block_dir);

  while (glm::dot(p, block_dir) > wrap_forward_progress) {
    p -= layout.block_offset;
    ++block_index;
  }
  while (glm::dot(p, block_dir) < wrap_backward_progress) {
    p += layout.block_offset;
    --block_index;
  }

  return p;
}

bool IsCameraPointInsideStaticWalkable(
    const glm::vec2 &world_point, float radius,
    const CanonicalCorridorLayout &layout) {
  int block_index = 0;
  const glm::vec2 local_point =
      WrapCameraPointToLocal(world_point, layout, radius, block_index);
  (void)block_index;

  const std::array<WalkableBox2D, kCorridorWalkableSectionCount>
      walkable_boxes =
          GetCorridorWalkableSections(layout, kCorridorHalfWidth, kCorridorZ1,
                                      radius);
  for (const WalkableBox2D &box : walkable_boxes) {
    if (IsInsideWalkableBox(box, local_point))
      return true;
  }
  return false;
}

glm::vec2 ClampCameraPointToStaticWalkable(
    const glm::vec2 &world_point, float radius,
    const CanonicalCorridorLayout &layout) {
  int block_index = 0;
  const glm::vec2 local_point =
      WrapCameraPointToLocal(world_point, layout, radius, block_index);
  const std::array<WalkableBox2D, kCorridorWalkableSectionCount>
      walkable_boxes =
          GetCorridorWalkableSections(layout, kCorridorHalfWidth, kCorridorZ1,
                                      radius);

  glm::vec2 best(
      ClampFloat(local_point.x, walkable_boxes[0].min_x,
                 walkable_boxes[0].max_x),
      ClampFloat(local_point.y, walkable_boxes[0].min_z,
                 walkable_boxes[0].max_z));
  float best_dist2 = glm::dot(best - local_point, best - local_point);

  for (const WalkableBox2D &box : walkable_boxes) {
    const glm::vec2 candidate(ClampFloat(local_point.x, box.min_x, box.max_x),
                              ClampFloat(local_point.y, box.min_z, box.max_z));
    const float dist2 =
        glm::dot(candidate - local_point, candidate - local_point);
    if (dist2 < best_dist2) {
      best = candidate;
      best_dist2 = dist2;
    }
  }

  return best + static_cast<float>(block_index) * layout.block_offset;
}

float FindVisibleThirdPersonBoomFraction(
    const glm::vec2 &target_ground, const glm::vec2 &desired_ground,
    float radius, const CanonicalCorridorLayout &layout) {
  const glm::vec2 boom = desired_ground - target_ground;
  const float boom_length = glm::length(boom);
  if (boom_length < 0.0001f)
    return 0.0f;

  float last_clear_t = 0.0f;
  for (int step = 1; step <= kThirdPersonCameraSweepSteps; ++step) {
    const float t =
        static_cast<float>(step) /
        static_cast<float>(kThirdPersonCameraSweepSteps);
    const glm::vec2 p = target_ground + boom * t;
    if (IsCameraPointInsideStaticWalkable(p, radius, layout)) {
      last_clear_t = t;
      continue;
    }

    float low = last_clear_t;
    float high = t;
    for (int i = 0; i < 12; ++i) {
      const float mid = 0.5f * (low + high);
      const glm::vec2 mid_point = target_ground + boom * mid;
      if (IsCameraPointInsideStaticWalkable(mid_point, radius, layout))
        low = mid;
      else
        high = mid;
    }

    const float backoff_t = kThirdPersonCameraBackoff / boom_length;
    return std::max(0.0f, low - backoff_t);
  }

  return 1.0f;
}
} // namespace

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
  glm::vec3 desired_camera = target - camera_front * kThirdPersonBoomDistance;
  desired_camera.y =
      std::max(0.65f, std::min(kCorridorHeight - 0.20f, desired_camera.y));

  const CanonicalCorridorLayout corridor_layout = GetCanonicalCorridorLayout();
  const glm::vec2 target_ground(target.x, target.z);
  const glm::vec2 desired_ground(desired_camera.x, desired_camera.z);
  const float boom_fraction = FindVisibleThirdPersonBoomFraction(
      target_ground, desired_ground, kThirdPersonCameraRadius,
      corridor_layout);

  glm::vec3 camera_position =
      target + (desired_camera - target) * boom_fraction;
  camera_position.y =
      std::max(0.65f, std::min(kCorridorHeight - 0.20f, camera_position.y));

  const glm::vec2 clamped_ground = ClampCameraPointToStaticWalkable(
      glm::vec2(camera_position.x, camera_position.z),
      kThirdPersonCameraRadius, corridor_layout);
  camera_position.x = clamped_ground.x;
  camera_position.z = clamped_ground.y;

  const float actual_boom_distance = glm::length(camera_position - target);
  g_PlayerCharacterHiddenByCamera =
      actual_boom_distance < kThirdPersonPlayerHideDistance;

  g_CameraPosition =
      glm::vec4(camera_position.x, camera_position.y, camera_position.z, 1.0f);
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

  const float movement_speed =
      sprint_active ? GetPlayerThirdPersonShiftSprintSpeed()
                    : g_PlayerCharacter.speed;

  glm::vec4 camera_front_4 = ComputeCameraFrontVector();
  glm::vec3 camera_front(camera_front_4.x, 0.0f, camera_front_4.z);
  if (glm::length(camera_front) < 0.0001f)
    camera_front = glm::vec3(0.0f, 0.0f, -1.0f);
  else
    camera_front = glm::normalize(camera_front);

  const glm::vec3 camera_right = glm::normalize(
      glm::cross(camera_front, glm::vec3(0.0f, 1.0f, 0.0f)));
  glm::vec3 movement(0.0f);
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    movement += camera_front;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    movement -= camera_front;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    movement -= camera_right;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    movement += camera_right;

  if (glm::length(movement) > 0.0001f) {
    movement = glm::normalize(movement);
    const glm::vec3 displacement = movement * movement_speed * delta_time;
    g_CameraPosition +=
        glm::vec4(displacement.x, displacement.y, displacement.z, 0.0f);
  }

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
  if (width <= 0 || height <= 0)
    return;

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

  // Atalho para tela cheia (F11)
  static int windowed_xpos, windowed_ypos, windowed_width, windowed_height;
  if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
    GLFWmonitor* monitor = glfwGetWindowMonitor(window);
    if (monitor) {
      // Atualmente em tela cheia, voltar para o modo janela
      glfwSetWindowMonitor(window, NULL, windowed_xpos, windowed_ypos, windowed_width, windowed_height, GLFW_DONT_CARE);
    } else {
      // Atualmente em modo janela, salvar dimensões e ir para tela cheia
      glfwGetWindowPos(window, &windowed_xpos, &windowed_ypos);
      glfwGetWindowSize(window, &windowed_width, &windowed_height);
      GLFWmonitor* primary = glfwGetPrimaryMonitor();
      const GLFWvidmode* mode = glfwGetVideoMode(primary);
      glfwSetWindowMonitor(window, primary, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
  }
}

// Definimos o callback para impressão de erros da GLFW no terminal
void ErrorCallback(int error, const char *description) {
  fprintf(stderr, "ERROR: GLFW: %s\n", description);
}
