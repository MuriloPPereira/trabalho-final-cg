#ifndef ENGINE_CAMERA_H
#define ENGINE_CAMERA_H

#include <glm/vec4.hpp>

struct GLFWwindow;

extern float g_ScreenRatio;
extern float g_AngleX;
extern float g_AngleY;
extern float g_AngleZ;
extern bool g_LeftMouseButtonPressed;
extern bool g_RightMouseButtonPressed;
extern bool g_MiddleMouseButtonPressed;
extern glm::vec4 g_CameraPosition;
extern float g_CameraYaw;
extern float g_CameraPitch;
extern bool g_FirstMouseInput;
extern float g_ForearmAngleZ;
extern float g_ForearmAngleX;
extern float g_TorsoPositionX;
extern float g_TorsoPositionY;
extern bool g_UsePerspectiveProjection;
extern bool g_ShowInfoText;
extern bool g_UseThirdPersonCamera;
extern bool g_PlayerInputEnabled;
extern double g_LastCursorPosX;
extern double g_LastCursorPosY;

glm::vec4 ComputeCameraFrontVector();
glm::vec4 ComputeCameraViewVector();
void UpdateThirdPersonCameraFromPlayer();
void UpdateCameraFromInput(GLFWwindow *window, float delta_time);
void FramebufferSizeCallback(GLFWwindow *window, int width, int height);
void ErrorCallback(int error, const char *description);
void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mode);
void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
void CursorPosCallback(GLFWwindow *window, double xpos, double ypos);
void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset);

#endif
