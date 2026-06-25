#include "engine/Texture.h"

#include "utils/FileUtils.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <stb_image.h>

GLuint g_NumLoadedTextures = 0;

namespace {
struct LoadedTexture {
  GLuint texture_id;
  GLuint sampler_id;
};

std::vector<LoadedTexture> g_LoadedTextures;
} // namespace

GLuint RegisterLoadedTexture(GLuint texture_id, GLuint sampler_id) {
  g_LoadedTextures.push_back(LoadedTexture{texture_id, sampler_id});
  g_NumLoadedTextures = static_cast<GLuint>(g_LoadedTextures.size());
  return g_NumLoadedTextures - 1;
}

// Função que carrega uma imagem para ser utilizada como textura
void LoadTextureImage(const char *filename, GLint wrap_s, GLint wrap_t) {
  const std::string requested_path = std::string("data/") + filename;
  const std::string fullpath = ResolveExistingPath(requested_path.c_str());
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

  const GLuint upload_texture_unit = 0;
  glActiveTexture(GL_TEXTURE0 + upload_texture_unit);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB,
               GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);
  glBindSampler(upload_texture_unit, 0);
  glBindTexture(GL_TEXTURE_2D, 0);

  stbi_image_free(data);

  RegisterLoadedTexture(texture_id, sampler_id);
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
  const GLuint upload_texture_unit = 0;
  glActiveTexture(GL_TEXTURE0 + upload_texture_unit);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE,
               pixel);
  glBindSampler(upload_texture_unit, 0);
  glBindTexture(GL_TEXTURE_2D, 0);

  RegisterLoadedTexture(texture_id, sampler_id);
}

void BindLoadedTexture(GLuint texture_index, GLuint texture_unit) {
  if (texture_index >= g_LoadedTextures.size()) {
    fprintf(stderr,
            "ERROR: Texture index %u is not loaded; binding texture 0.\n",
            texture_index);
    glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindSampler(texture_unit, 0);
    return;
  }

  const LoadedTexture &texture = g_LoadedTextures[texture_index];
  glActiveTexture(GL_TEXTURE0 + texture_unit);
  glBindTexture(GL_TEXTURE_2D, texture.texture_id);
  glBindSampler(texture_unit, texture.sampler_id);
}
