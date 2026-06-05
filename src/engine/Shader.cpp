#include "engine/Shader.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

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
GLint g_light_position_uniforms[kMaxLights];
GLint g_light_color_uniforms[kMaxLights];
GLint g_light_ambient_strength_uniforms[kMaxLights];
GLint g_light_diffuse_strength_uniforms[kMaxLights];
GLint g_light_specular_strength_uniforms[kMaxLights];
GLint g_light_constant_uniforms[kMaxLights];
GLint g_light_linear_uniforms[kMaxLights];
GLint g_light_quadratic_uniforms[kMaxLights];

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
