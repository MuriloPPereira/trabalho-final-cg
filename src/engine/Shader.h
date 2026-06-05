#ifndef ENGINE_SHADER_H
#define ENGINE_SHADER_H

#include <glad/glad.h>
#include "utils/Constants.h"

extern GLuint g_GpuProgramID;
extern GLint g_model_uniform;
extern GLint g_view_uniform;
extern GLint g_projection_uniform;
extern GLint g_use_skinning_uniform;
extern GLint g_bone_matrices_uniform;
extern GLint g_bbox_min_uniform;
extern GLint g_bbox_max_uniform;
extern GLint g_camera_position_uniform;
extern GLint g_material_diffuse_uniform;
extern GLint g_material_specular_strength_uniform;
extern GLint g_material_shininess_uniform;
extern GLint g_material_ambient_strength_uniform;
extern GLint g_material_uv_scale_uniform;
extern GLint g_material_uv_offset_uniform;
extern GLint g_num_lights_uniform;
extern GLint g_light_position_uniforms[kMaxLights];
extern GLint g_light_color_uniforms[kMaxLights];
extern GLint g_light_ambient_strength_uniforms[kMaxLights];
extern GLint g_light_diffuse_strength_uniforms[kMaxLights];
extern GLint g_light_specular_strength_uniforms[kMaxLights];
extern GLint g_light_constant_uniforms[kMaxLights];
extern GLint g_light_linear_uniforms[kMaxLights];
extern GLint g_light_quadratic_uniforms[kMaxLights];

void LoadShadersFromFiles();
GLuint LoadShader_Vertex(const char *filename);
GLuint LoadShader_Fragment(const char *filename);
void LoadShader(const char *filename, GLuint shader_id);
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id);

#endif
