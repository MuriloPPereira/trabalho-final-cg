#ifndef ENGINE_TEXTURE_H
#define ENGINE_TEXTURE_H

#include <glad/glad.h>

extern GLuint g_NumLoadedTextures;

void LoadTextureImage(const char *filename, GLint wrap_s, GLint wrap_t);
void CreateSolidColorTexture(unsigned char r, unsigned char g, unsigned char b);

#endif
