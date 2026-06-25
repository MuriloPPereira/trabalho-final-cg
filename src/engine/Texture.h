#ifndef ENGINE_TEXTURE_H
#define ENGINE_TEXTURE_H

#include <glad/glad.h>

extern GLuint g_NumLoadedTextures;

GLuint RegisterLoadedTexture(GLuint texture_id, GLuint sampler_id);
void LoadTextureImage(const char *filename, GLint wrap_s, GLint wrap_t);
void CreateSolidColorTexture(unsigned char r, unsigned char g, unsigned char b);
void BindLoadedTexture(GLuint texture_index, GLuint texture_unit);

#endif
