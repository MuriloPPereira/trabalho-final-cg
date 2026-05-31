//     Universidade Federal do Rio Grande do Sul
//             Instituto de Informática
//       Departamento de Informática Aplicada
//
//    INF01047 Computação Gráfica e Visualização I
//               Prof. Eduardo Gastal
//
//     CÓDIGO BASE PARA O TRABALHO FINAL
//

// Arquivos "headers" padrões de C podem ser incluídos em um
// programa C++, sendo necessário somente adicionar o caractere
// "c" antes de seu nome, e remover o sufixo ".h". Exemplo:
//    #include <stdio.h> // Em C
//  vira
//    #include <cstdio> // Em C++
//
#include <cmath>
#include <cstdio>
#include <cstdlib>

// Headers abaixo são específicos de C++
#include <set>
#include <map>
#include <stack>
#include <string>
#include <vector>
#include <limits>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// Headers das bibliotecas OpenGL
#include <glad/glad.h>   // Criação de contexto OpenGL 3.3
#include <GLFW/glfw3.h>  // Criação de janelas do sistema operacional

// Headers da biblioteca GLM: criação de matrizes e vetores.
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Headers da biblioteca para carregar modelos obj
#include <tiny_obj_loader.h>

#include <stb_image.h>

// Headers locais, definidos na pasta "include/"
#include "utils.h"
#include "matrices.h"

// Estrutura que representa um modelo geométrico carregado a partir de um
// arquivo ".obj". Veja https://en.wikipedia.org/wiki/Wavefront_.obj_file .
struct ObjModel
{
    tinyobj::attrib_t                 attrib;
    std::vector<tinyobj::shape_t>     shapes;
    std::vector<tinyobj::material_t>  materials;

    // Este construtor lê o modelo de um arquivo utilizando a biblioteca tinyobjloader.
    // Veja: https://github.com/syoyo/tinyobjloader
    ObjModel(const char* filename, const char* basepath = NULL, bool triangulate = true)
    {
        printf("Carregando objetos do arquivo \"%s\"...\n", filename);

        // Se basepath == NULL, então setamos basepath como o dirname do
        // filename, para que os arquivos MTL sejam corretamente carregados caso
        // estejam no mesmo diretório dos arquivos OBJ.
        std::string fullpath(filename);
        std::string dirname;
        if (basepath == NULL)
        {
            auto i = fullpath.find_last_of("/");
            if (i != std::string::npos)
            {
                dirname = fullpath.substr(0, i+1);
                basepath = dirname.c_str();
            }
        }

        std::string warn;
        std::string err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, basepath, triangulate);

        if (!err.empty())
            fprintf(stderr, "\n%s\n", err.c_str());

        if (!ret)
            throw std::runtime_error("Erro ao carregar modelo.");

        for (size_t shape = 0; shape < shapes.size(); ++shape)
        {
            if (shapes[shape].name.empty())
            {
                fprintf(stderr,
                        "*********************************************\n"
                        "Erro: Objeto sem nome dentro do arquivo '%s'.\n"
                        "Veja https://www.inf.ufrgs.br/~eslgastal/fcg-faq-etc.html#Modelos-3D-no-formato-OBJ .\n"
                        "*********************************************\n",
                    filename);
                throw std::runtime_error("Objeto sem nome.");
            }
            printf("- Objeto '%s'\n", shapes[shape].name.c_str());
        }

        printf("OK.\n");
    }
};


// Declaração de funções utilizadas para pilha de matrizes de modelagem.
void PushMatrix(glm::mat4 M);
void PopMatrix(glm::mat4& M);

struct Material;
struct PointLight;

// Declaração de várias funções utilizadas em main().  Essas estão definidas
// logo após a definição de main() neste arquivo.
void BuildTrianglesAndAddToVirtualScene(ObjModel*); // Constrói representação de um ObjModel como malha de triângulos para renderização
void BuildCorridorAndAddToVirtualScene(); // Constrói um corredor procedural simples (chão, teto e paredes)
void BuildCornerAndAddToVirtualScene(); // Constrói duas quinas procedurais 4x4 (esquerda e direita)
void BuildPostersAndAddToVirtualScene(); // Constrói quads para os posters na parede esquerda
void ComputeNormals(ObjModel* model); // Computa normais de um ObjModel, caso não existam.
void LoadShadersFromFiles(); // Carrega os shaders de vértice e fragmento, criando um programa de GPU
void LoadTextureImage(const char* filename, GLint wrap_s, GLint wrap_t); // Função que carrega imagens de textura
void DrawVirtualObject(const char* object_name); // Desenha um objeto armazenado em g_VirtualScene
void ApplyMaterial(const struct Material& material);
void SetPointLights(const std::vector<struct PointLight>& lights);
GLuint LoadShader_Vertex(const char* filename);   // Carrega um vertex shader
GLuint LoadShader_Fragment(const char* filename); // Carrega um fragment shader
void LoadShader(const char* filename, GLuint shader_id); // Função utilizada pelas duas acima
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id); // Cria um programa de GPU
void PrintObjModelInfo(ObjModel*); // Função para debugging

// Declaração de funções auxiliares para renderizar texto dentro da janela
// OpenGL. Estas funções estão definidas no arquivo "textrendering.cpp".
void TextRendering_Init();
float TextRendering_LineHeight(GLFWwindow* window);
float TextRendering_CharWidth(GLFWwindow* window);
void TextRendering_PrintString(GLFWwindow* window, const std::string &str, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrix(GLFWwindow* window, glm::mat4 M, float x, float y, float scale = 1.0f);
void TextRendering_PrintVector(GLFWwindow* window, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProduct(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductMoreDigits(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductDivW(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);

// Funções abaixo renderizam como texto na janela OpenGL algumas matrizes e
// outras informações do programa. Definidas após main().
void TextRendering_ShowModelViewProjection(GLFWwindow* window, glm::mat4 projection, glm::mat4 view, glm::mat4 model, glm::vec4 p_model);
void TextRendering_ShowEulerAngles(GLFWwindow* window);
void TextRendering_ShowProjection(GLFWwindow* window);
void TextRendering_ShowFramesPerSecond(GLFWwindow* window);

// Funções callback para comunicação com o sistema operacional e interação do
// usuário. Veja mais comentários nas definições das mesmas, abaixo.
void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void ErrorCallback(int error, const char* description);
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void UpdateCameraFromInput(GLFWwindow* window, float delta_time);
glm::vec4 ComputeCameraFrontVector();

// Definimos uma estrutura que armazenará dados necessários para renderizar
// cada objeto da cena virtual.
struct SceneObject
{
    std::string  name;        // Nome do objeto
    size_t       first_index; // Índice do primeiro vértice dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    size_t       num_indices; // Número de índices do objeto dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    GLenum       rendering_mode; // Modo de rasterização (GL_TRIANGLES, GL_TRIANGLE_STRIP, etc.)
    GLuint       vertex_array_object_id; // ID do VAO onde estão armazenados os atributos do modelo
    glm::vec3    bbox_min; // Axis-Aligned Bounding Box do objeto
    glm::vec3    bbox_max;
};

struct Material
{
    GLuint diffuse_texture_unit;
    float specular_strength;
    float shininess;
    float ambient_strength;
    glm::vec2 uv_scale;
};

struct PointLight
{
    glm::vec3 position;
    glm::vec3 color;
    float ambient_strength;
    float diffuse_strength;
    float specular_strength;
    float constant;
    float linear;
    float quadratic;
};

// Abaixo definimos variáveis globais utilizadas em várias funções do código.

// A cena virtual é uma lista de objetos nomeados, guardados em um dicionário
// (map).  Veja dentro da função BuildTrianglesAndAddToVirtualScene() como que são incluídos
// objetos dentro da variável g_VirtualScene, e veja na função main() como
// estes são acessados.
std::map<std::string, SceneObject> g_VirtualScene;

// Pilha que guardará as matrizes de modelagem.
std::stack<glm::mat4>  g_MatrixStack;

// Razão de proporção da janela (largura/altura). Veja função FramebufferSizeCallback().
float g_ScreenRatio = 1.0f;

// Ângulos de Euler que controlam a rotação de um dos cubos da cena virtual
float g_AngleX = 0.0f;
float g_AngleY = 0.0f;
float g_AngleZ = 0.0f;

// "g_LeftMouseButtonPressed = true" se o usuário está com o botão esquerdo do mouse
// pressionado no momento atual. Veja função MouseButtonCallback().
bool g_LeftMouseButtonPressed = false;
bool g_RightMouseButtonPressed = false; // Análogo para botão direito do mouse
bool g_MiddleMouseButtonPressed = false; // Análogo para botão do meio do mouse

// Câmera em primeira pessoa.
glm::vec4 g_CameraPosition = glm::vec4(0.0f, 1.6f, 1.0f, 1.0f);
float g_CameraYaw = 0.0f;
float g_CameraPitch = 0.0f;
bool g_FirstMouseInput = true;

// Variáveis que controlam rotação do antebraço
float g_ForearmAngleZ = 0.0f;
float g_ForearmAngleX = 0.0f;

// Variáveis que controlam translação do torso
float g_TorsoPositionX = 0.0f;
float g_TorsoPositionY = 0.0f;

// Variável que controla o tipo de projeção utilizada: perspectiva ou ortográfica.
bool g_UsePerspectiveProjection = true;

// Variável que controla se o texto informativo será mostrado na tela.
bool g_ShowInfoText = true;

// Variáveis que definem um programa de GPU (shaders). Veja função LoadShadersFromFiles().
GLuint g_GpuProgramID = 0;
GLint g_model_uniform;
GLint g_view_uniform;
GLint g_projection_uniform;
GLint g_bbox_min_uniform;
GLint g_bbox_max_uniform;
GLint g_camera_position_uniform;
GLint g_material_diffuse_uniform;
GLint g_material_specular_strength_uniform;
GLint g_material_shininess_uniform;
GLint g_material_ambient_strength_uniform;
GLint g_material_uv_scale_uniform;
GLint g_num_lights_uniform;

const int kMaxLights = 24;
GLint g_light_position_uniforms[kMaxLights];
GLint g_light_color_uniforms[kMaxLights];
GLint g_light_ambient_strength_uniforms[kMaxLights];
GLint g_light_diffuse_strength_uniforms[kMaxLights];
GLint g_light_specular_strength_uniforms[kMaxLights];
GLint g_light_constant_uniforms[kMaxLights];
GLint g_light_linear_uniforms[kMaxLights];
GLint g_light_quadratic_uniforms[kMaxLights];

const float kCorridorHalfWidth = 2.0f;
const float kCorridorHeight = 3.0f;
const float kCorridorLength = 40.0f;
const float kCorridorZ0 = 0.0f;
const float kCorridorZ1 = -kCorridorLength;
const float kCornerLength = 4.0f;
const float kConnectorLength = 10.0f;
const int kPosterCount = 4;
const int kLampCount = 12;
const char* kPosterNames[kPosterCount] = {"poster_0", "poster_1", "poster_2", "poster_3"};

// Número de texturas carregadas pela função LoadTextureImage()
GLuint g_NumLoadedTextures = 0;

int main(int argc, char* argv[])
{
    // Inicializamos a biblioteca GLFW, utilizada para criar uma janela do
    // sistema operacional, onde poderemos renderizar com OpenGL.
    int success = glfwInit();
    if (!success)
    {
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
    GLFWwindow* window;
    window = glfwCreateWindow(800, 600, "INF01047 - Seu Cartao - Seu Nome", NULL, NULL);
    if (!window)
    {
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
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    // Definimos a função de callback que será chamada sempre que a janela for
    // redimensionada, por consequência alterando o tamanho do "framebuffer"
    // (região de memória onde são armazenados os pixels da imagem).
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    FramebufferSizeCallback(window, 800, 600); // Forçamos a chamada do callback acima, para definir g_ScreenRatio.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Imprimimos no terminal informações sobre a GPU do sistema
    const GLubyte *vendor      = glGetString(GL_VENDOR);
    const GLubyte *renderer    = glGetString(GL_RENDERER);
    const GLubyte *glversion   = glGetString(GL_VERSION);
    const GLubyte *glslversion = glGetString(GL_SHADING_LANGUAGE_VERSION);

    printf("GPU: %s, %s, OpenGL %s, GLSL %s\n", vendor, renderer, glversion, glslversion);

    // Carregamos os shaders de vértices e de fragmentos que serão utilizados
    // para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
    //
    LoadShadersFromFiles();

    // Carregamos imagens para serem utilizadas como textura (caminhos relativos a data/)
    LoadTextureImage("wall.jpg", GL_REPEAT, GL_REPEAT); // 0
    LoadTextureImage("floor.jpg", GL_REPEAT, GL_REPEAT); // 1
    LoadTextureImage("ceiling.jpg", GL_REPEAT, GL_REPEAT); // 2
    LoadTextureImage("poster1.jpg", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE); // 3
    LoadTextureImage("poster2.jpg", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE); // 4
    LoadTextureImage("poster3.jpg", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE); // 5
    LoadTextureImage("poster4.jpg", GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE); // 6

    BuildCorridorAndAddToVirtualScene();
    BuildCornerAndAddToVirtualScene();
    BuildPostersAndAddToVirtualScene();

    const GLuint kWallTextureUnit = 0;
    const GLuint kFloorTextureUnit = 1;
    const GLuint kCeilingTextureUnit = 2;
    const GLuint kPosterTextureUnits[kPosterCount] = {3, 4, 5, 6};

    const float floor_tile_size = 0.5f;
    const float ceiling_tile_size = 2.5f;
    const float wall_tile_size_z = 2.0f;
    const float wall_tile_size_y = 1.5f;

    const glm::vec2 floor_uv_scale((2.0f * kCorridorHalfWidth) / floor_tile_size,
                                   kCorridorLength / floor_tile_size);
    const glm::vec2 ceiling_uv_scale((2.0f * kCorridorHalfWidth) / ceiling_tile_size,
                                     kCorridorLength / ceiling_tile_size);
    const glm::vec2 wall_uv_scale(kCorridorLength / wall_tile_size_z,
                                  kCorridorHeight / wall_tile_size_y);

    Material wall_material;
    wall_material.diffuse_texture_unit = kWallTextureUnit;
    wall_material.specular_strength = 0.9f;
    wall_material.shininess = 96.0f;
    wall_material.ambient_strength = 0.05f;
    wall_material.uv_scale = wall_uv_scale;

    Material floor_material;
    floor_material.diffuse_texture_unit = kFloorTextureUnit;
    floor_material.specular_strength = 0.35f;
    floor_material.shininess = 48.0f;
    floor_material.ambient_strength = 0.04f;
    floor_material.uv_scale = floor_uv_scale;

    Material ceiling_material;
    ceiling_material.diffuse_texture_unit = kCeilingTextureUnit;
    ceiling_material.specular_strength = 0.15f;
    ceiling_material.shininess = 20.0f;
    ceiling_material.ambient_strength = 0.03f;
    ceiling_material.uv_scale = ceiling_uv_scale;

    std::vector<Material> poster_materials;
    poster_materials.reserve(kPosterCount);
    for (int i = 0; i < kPosterCount; ++i)
    {
        Material poster_material;
        poster_material.diffuse_texture_unit = kPosterTextureUnits[i];
        poster_material.specular_strength = 0.10f;
        poster_material.shininess = 24.0f;
        poster_material.ambient_strength = 0.05f;
        poster_material.uv_scale = glm::vec2(1.0f, 1.0f);
        poster_materials.push_back(poster_material);
    }
const float corridor2_offset_x = -(2.0f * kCorridorHalfWidth + kConnectorLength);
    const float second_corridor_z_offset = kCorridorZ1 - kCornerLength; 
    const glm::vec2 block_offset(corridor2_offset_x, second_corridor_z_offset);
    const float connector_center_z = kCorridorZ1 - 0.5f * kCornerLength;

    std::vector<PointLight> corridor_lights;
    corridor_lights.reserve(kMaxLights);

    auto make_light = [&](const glm::vec3& position)
    {
        PointLight light;
        light.position = position;
        light.color = glm::vec3(1.0f, 0.98f, 0.92f);
        light.ambient_strength = 0.03f;
        light.diffuse_strength = 1.0f;
        light.specular_strength = 1.0f;
        light.constant = 1.0f;
        light.linear = 0.14f;
        light.quadratic = 0.07f;
        return light;
    };

    // Lambda to generate the 6 lights for a single modular block
    auto add_block_lights = [&](const glm::vec3& offset)
    {
        // 4 lights in the straight corridor
        const float straight_spacing = kCorridorLength / 5.0f;
        for (int i = 0; i < 4; ++i)
        {
            corridor_lights.push_back(make_light(offset + glm::vec3(0.0f, kCorridorHeight - 0.15f, -(i + 1) * straight_spacing)));
        }

        // 2 lights in the connector corridor
        corridor_lights.push_back(make_light(offset + glm::vec3(-kCorridorHalfWidth - (kConnectorLength / 3.0f), kCorridorHeight - 0.15f, connector_center_z)));
        corridor_lights.push_back(make_light(offset + glm::vec3(-kCorridorHalfWidth - (2.0f * kConnectorLength / 3.0f), kCorridorHeight - 0.15f, connector_center_z)));
    };

    // Apply lights to all three treadmill tiles
    add_block_lights(glm::vec3(-block_offset.x, 0.0f, -block_offset.y)); // Block -1 (Behind)
    add_block_lights(glm::vec3(0.0f, 0.0f, 0.0f));                       // Block 0 (Center)
    add_block_lights(glm::vec3(block_offset.x, 0.0f, block_offset.y));   // Block 1 (Ahead)

    // Inicializamos o código para renderização de texto.
    TextRendering_Init();
    // Habilitamos o Z-buffer. Veja slides 104-116 do documento Aula_09_Projecoes.pdf.
    glEnable(GL_DEPTH_TEST);

    // Habilitamos o Backface Culling. Veja slides 8-13 do documento Aula_02_Fundamentos_Matematicos.pdf, slides 23-34 do documento Aula_13_Clipping_and_Culling.pdf e slides 112-123 do documento Aula_14_Laboratorio_3_Revisao.pdf.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Ficamos em um loop infinito, renderizando, até que o usuário feche a janela
    float last_frame_time = (float)glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        float current_frame_time = (float)glfwGetTime();
        float delta_time = current_frame_time - last_frame_time;
        last_frame_time = current_frame_time;
        UpdateCameraFromInput(window, delta_time);

        // Aqui executamos as operações de renderização

        // Definimos a cor do "fundo" do framebuffer como branco.  Tal cor é
        // definida como coeficientes RGBA: Red, Green, Blue, Alpha; isto é:
        // Vermelho, Verde, Azul, Alpha (valor de transparência).
        // Conversaremos sobre sistemas de cores nas aulas de Modelos de Iluminação.
        //
        //           R     G     B     A
        glClearColor(0.07f, 0.07f, 0.08f, 1.0f);

        // "Pintamos" todos os pixels do framebuffer com a cor definida acima,
        // e também resetamos todos os pixels do Z-buffer (depth buffer).
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Pedimos para a GPU utilizar o programa de GPU criado acima (contendo
        // os shaders de vértice e fragmentos).
        glUseProgram(g_GpuProgramID);

        glm::vec4 camera_position_c  = g_CameraPosition;
        glm::vec4 camera_front_vector = ComputeCameraFrontVector();
        glm::vec4 camera_lookat_l    = camera_position_c + camera_front_vector;
        glm::vec4 camera_view_vector = camera_lookat_l - camera_position_c;
        glm::vec4 camera_up_vector   = glm::vec4(0.0f,1.0f,0.0f,0.0f);

        // Computamos a matriz "View" utilizando os parâmetros da câmera para
        // definir o sistema de coordenadas da câmera.  Veja slides 2-14, 184-190 e 236-242 do documento Aula_08_Sistemas_de_Coordenadas.pdf.
        glm::mat4 view = Matrix_Camera_View(camera_position_c, camera_view_vector, camera_up_vector);

        // Agora computamos a matriz de Projeção.
        glm::mat4 projection;

        // Note que, no sistema de coordenadas da câmera, os planos near e far
        // estão no sentido negativo! Veja slides 176-204 do documento Aula_09_Projecoes.pdf.
        float nearplane = -0.1f;  // Posição do "near plane"
        float farplane  = -(2.0f * kCorridorLength + 10.0f); // Posição do "far plane"

        if (g_UsePerspectiveProjection)
        {
            // Projeção Perspectiva.
            // Para definição do field of view (FOV), veja slides 205-215 do documento Aula_09_Projecoes.pdf.
            float field_of_view = 3.141592 / 3.0f;
            projection = Matrix_Perspective(field_of_view, g_ScreenRatio, nearplane, farplane);
        }
        else
        {
            // Projeção Ortográfica.
            // Para definição dos valores l, r, b, t ("left", "right", "bottom", "top"),
            // PARA PROJEÇÃO ORTOGRÁFICA veja slides 219-224 do documento Aula_09_Projecoes.pdf.
            // Projeção ortográfica fixa.
            float t = 1.5f;
            float b = -t;
            float r = t*g_ScreenRatio;
            float l = -r;
            projection = Matrix_Orthographic(l, r, b, t, nearplane, farplane);
        }

        // Enviamos as matrizes "view" e "projection" para a placa de vídeo
        // (GPU). Veja o arquivo "shader_vertex.glsl", onde estas são
        // efetivamente aplicadas em todos os pontos.
        glUniformMatrix4fv(g_view_uniform       , 1 , GL_FALSE , glm::value_ptr(view));
        glUniformMatrix4fv(g_projection_uniform , 1 , GL_FALSE , glm::value_ptr(projection));

        glUniform4f(g_camera_position_uniform, camera_position_c.x, camera_position_c.y, camera_position_c.z, camera_position_c.w);
        SetPointLights(corridor_lights);

        auto draw_straight_corridor = [&](const glm::mat4& corridor_model,
                                          bool draw_posters,
                                          const Material& floor_mat,
                                          const Material& ceiling_mat,
                                          const Material& wall_mat)
        {
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(corridor_model));

            ApplyMaterial(floor_mat);
            DrawVirtualObject("corridor_floor");

            ApplyMaterial(ceiling_mat);
            DrawVirtualObject("corridor_ceiling");

            ApplyMaterial(wall_mat);
            DrawVirtualObject("corridor_wall_left");
            DrawVirtualObject("corridor_wall_right");

            if (draw_posters)
            {
                for (int i = 0; i < kPosterCount; ++i)
                {
                    ApplyMaterial(poster_materials[i]);
                    DrawVirtualObject(kPosterNames[i]);
                }
            }
        };


        // Materiais para o conector curto.
        Material connector_floor_material = floor_material;
        Material connector_ceiling_material = ceiling_material;
        Material connector_wall_material = wall_material;
        const float connector_uv_factor = kConnectorLength / kCorridorLength;
        connector_floor_material.uv_scale.y *= connector_uv_factor;
        connector_ceiling_material.uv_scale.y *= connector_uv_factor;
        connector_wall_material.uv_scale.x *= connector_uv_factor;

        // [NOVO] Materiais específicos para as Quinas (tamanho de 4x4)
        Material corner_floor_material = floor_material;
        Material corner_ceiling_material = ceiling_material;
        Material corner_wall_material = wall_material;
        
        const float corner_uv_factor = kCornerLength / kCorridorLength; // 4.0 / 40.0 = 0.1
        corner_floor_material.uv_scale.y *= corner_uv_factor;
        corner_ceiling_material.uv_scale.y *= corner_uv_factor;
        corner_wall_material.uv_scale.x *= corner_uv_factor;

        // (1) Modular block (tile): corredor reto + quina esquerda + conector + quina direita.
        // O próximo tile começa em (corridor2_offset_x, second_corridor_z_offset) relativo ao tile atual.
        auto draw_modular_block = [&](const glm::mat4& base_transform)
        {
            // Corredor reto principal (eixo -Z) deste tile.
            draw_straight_corridor(base_transform, true, floor_material, ceiling_material, wall_material);

            // Quina esquerda no final do corredor: base_transform * T(0,0,kCorridorZ1).
            glm::mat4 m = base_transform * Matrix_Translate(0.0f, 0.0f, kCorridorZ1);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(m));

            ApplyMaterial(corner_floor_material);   DrawVirtualObject("corner_left_floor");
            ApplyMaterial(corner_ceiling_material); DrawVirtualObject("corner_left_ceiling");
            ApplyMaterial(corner_wall_material);
            DrawVirtualObject("corner_left_wall_back");
            DrawVirtualObject("corner_left_wall_right");

            // Conector curto (eixo -X): base_transform * T(...) * R_y(+90°) * S(...).
            m = base_transform
              * Matrix_Translate(-kCorridorHalfWidth, 0.0f, connector_center_z)
              * Matrix_Rotate_Y(+3.141592f / 2.0f)
              * Matrix_Scale(1.0f, 1.0f, kConnectorLength / kCorridorLength);
            draw_straight_corridor(m, false, connector_floor_material, connector_ceiling_material, connector_wall_material);

            // Quina direita no fim do conector: base_transform * T(corridor2_offset_x,0,kCorridorZ1).
            m = base_transform * Matrix_Translate(corridor2_offset_x, 0.0f, kCorridorZ1);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(m));

            ApplyMaterial(corner_floor_material);   DrawVirtualObject("corner_right_floor");
            ApplyMaterial(corner_ceiling_material); DrawVirtualObject("corner_right_ceiling");
            ApplyMaterial(corner_wall_material);
            DrawVirtualObject("corner_right_wall_front");
            DrawVirtualObject("corner_right_wall_left");
        };

        // (2) 3-Tile Treadmill: desenha o bloco anterior, o atual e o próximo.
        draw_modular_block(Matrix_Translate(-corridor2_offset_x, 0.0f, -second_corridor_z_offset)); // Block -1 (atrás)
        draw_modular_block(Matrix_Identity());                                                      // Block 0 (centro)
        draw_modular_block(Matrix_Translate(corridor2_offset_x, 0.0f, second_corridor_z_offset));    // Block +1 (à frente)

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
        // Veja o link: https://en.wikipedia.org/w/index.php?title=Multiple_buffering&oldid=793452829#Double_buffering_in_computer_graphics
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

// Função que carrega uma imagem para ser utilizada como textura
void LoadTextureImage(const char* filename, GLint wrap_s, GLint wrap_t)
{
    std::string fullpath = std::string("../../data/") + filename;
    printf("Carregando imagem \"%s\"... ", fullpath.c_str());

    // Primeiro fazemos a leitura da imagem do disco
    stbi_set_flip_vertically_on_load(true);
    int width;
    int height;
    int channels;
    unsigned char *data = stbi_load(fullpath.c_str(), &width, &height, &channels, 3);

    if ( data == NULL )
    {
        fprintf(stderr, "ERROR: Cannot open image file \"%s\".\n", fullpath.c_str());
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
    glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Agora enviamos a imagem lida do disco para a GPU
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    GLuint textureunit = g_NumLoadedTextures;
    glActiveTexture(GL_TEXTURE0 + textureunit);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindSampler(textureunit, sampler_id);

    stbi_image_free(data);

    g_NumLoadedTextures += 1;
}

void ApplyMaterial(const Material& material)
{
    glUniform1i(g_material_diffuse_uniform, material.diffuse_texture_unit);
    glUniform1f(g_material_specular_strength_uniform, material.specular_strength);
    glUniform1f(g_material_shininess_uniform, material.shininess);
    glUniform1f(g_material_ambient_strength_uniform, material.ambient_strength);
    glUniform2f(g_material_uv_scale_uniform, material.uv_scale.x, material.uv_scale.y);
}

void SetPointLights(const std::vector<PointLight>& lights)
{
    int count = static_cast<int>(std::min(lights.size(), static_cast<size_t>(kMaxLights)));
    glUniform1i(g_num_lights_uniform, count);

    for (int i = 0; i < count; ++i)
    {
        const PointLight& light = lights[i];
        glUniform3f(g_light_position_uniforms[i], light.position.x, light.position.y, light.position.z);
        glUniform3f(g_light_color_uniforms[i], light.color.x, light.color.y, light.color.z);
        glUniform1f(g_light_ambient_strength_uniforms[i], light.ambient_strength);
        glUniform1f(g_light_diffuse_strength_uniforms[i], light.diffuse_strength);
        glUniform1f(g_light_specular_strength_uniforms[i], light.specular_strength);
        glUniform1f(g_light_constant_uniforms[i], light.constant);
        glUniform1f(g_light_linear_uniforms[i], light.linear);
        glUniform1f(g_light_quadratic_uniforms[i], light.quadratic);
    }
}

// Função que desenha um objeto armazenado em g_VirtualScene. Veja definição
// dos objetos na função BuildTrianglesAndAddToVirtualScene().
void DrawVirtualObject(const char* object_name)
{
    // "Ligamos" o VAO. Informamos que queremos utilizar os atributos de
    // vértices apontados pelo VAO criado pela função BuildTrianglesAndAddToVirtualScene(). Veja
    // comentários detalhados dentro da definição de BuildTrianglesAndAddToVirtualScene().
    glBindVertexArray(g_VirtualScene[object_name].vertex_array_object_id);

    // Setamos as variáveis "bbox_min" e "bbox_max" do fragment shader
    // com os parâmetros da axis-aligned bounding box (AABB) do modelo.
    glm::vec3 bbox_min = g_VirtualScene[object_name].bbox_min;
    glm::vec3 bbox_max = g_VirtualScene[object_name].bbox_max;
    glUniform4f(g_bbox_min_uniform, bbox_min.x, bbox_min.y, bbox_min.z, 1.0f);
    glUniform4f(g_bbox_max_uniform, bbox_max.x, bbox_max.y, bbox_max.z, 1.0f);

    // Pedimos para a GPU rasterizar os vértices dos eixos XYZ
    // apontados pelo VAO como linhas. Veja a definição de
    // g_VirtualScene[""] dentro da função BuildTrianglesAndAddToVirtualScene(), e veja
    // a documentação da função glDrawElements() em
    // http://docs.gl/gl3/glDrawElements.
    glDrawElements(
        g_VirtualScene[object_name].rendering_mode,
        g_VirtualScene[object_name].num_indices,
        GL_UNSIGNED_INT,
        (void*)(g_VirtualScene[object_name].first_index * sizeof(GLuint))
    );

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

void BuildPostersAndAddToVirtualScene()
{
    struct PosterVertex
    {
        float px, py, pz, pw;
        float nx, ny, nz, nw;
        float u, v;
    };

    std::vector<PosterVertex> vertices;
    std::vector<GLuint> indices;

    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    auto add_poster_quad = [&](const std::string& name,
                               glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3,
                               glm::vec3 normal)
    {
        size_t first_index = indices.size();
        GLuint base_vertex = static_cast<GLuint>(vertices.size());

        auto push_vertex = [&](glm::vec3 p, float u, float v)
        {
            PosterVertex vertex;
            vertex.px = p.x; vertex.py = p.y; vertex.pz = p.z; vertex.pw = 1.0f;
            vertex.nx = normal.x; vertex.ny = normal.y; vertex.nz = normal.z; vertex.nw = 0.0f;
            vertex.u = u; vertex.v = v;
            vertices.push_back(vertex);
        };

        push_vertex(p0, 0.0f, 0.0f);
        push_vertex(p1, 1.0f, 0.0f);
        push_vertex(p2, 1.0f, 1.0f);
        push_vertex(p3, 0.0f, 1.0f);

        indices.push_back(base_vertex + 0);
        indices.push_back(base_vertex + 1);
        indices.push_back(base_vertex + 2);
        indices.push_back(base_vertex + 0);
        indices.push_back(base_vertex + 2);
        indices.push_back(base_vertex + 3);

        glm::vec3 bbox_min = p0;
        glm::vec3 bbox_max = p0;
        bbox_min.x = std::min(bbox_min.x, p1.x); bbox_min.y = std::min(bbox_min.y, p1.y); bbox_min.z = std::min(bbox_min.z, p1.z);
        bbox_max.x = std::max(bbox_max.x, p1.x); bbox_max.y = std::max(bbox_max.y, p1.y); bbox_max.z = std::max(bbox_max.z, p1.z);
        bbox_min.x = std::min(bbox_min.x, p2.x); bbox_min.y = std::min(bbox_min.y, p2.y); bbox_min.z = std::min(bbox_min.z, p2.z);
        bbox_max.x = std::max(bbox_max.x, p2.x); bbox_max.y = std::max(bbox_max.y, p2.y); bbox_max.z = std::max(bbox_max.z, p2.z);
        bbox_min.x = std::min(bbox_min.x, p3.x); bbox_min.y = std::min(bbox_min.y, p3.y); bbox_min.z = std::min(bbox_min.z, p3.z);
        bbox_max.x = std::max(bbox_max.x, p3.x); bbox_max.y = std::max(bbox_max.y, p3.y); bbox_max.z = std::max(bbox_max.z, p3.z);

        SceneObject object;
        object.name = name;
        object.first_index = first_index;
        object.num_indices = 6;
        object.rendering_mode = GL_TRIANGLES;
        object.vertex_array_object_id = vertex_array_object_id;
        object.bbox_min = bbox_min;
        object.bbox_max = bbox_max;
        g_VirtualScene[name] = object;
    };

    const float poster_width = 1.8f;
    const float poster_height = 1.5f;
    const float poster_center_y = 1.6f;
    const float poster_offset = 0.02f;
    const float poster_x = -kCorridorHalfWidth + poster_offset;
    const float spacing = kCorridorLength / (kPosterCount + 1);

    for (int i = 0; i < kPosterCount; ++i)
    {
        float center_z = -(i + 1) * spacing;
        float z_near = center_z + poster_width * 0.5f;
        float z_far = center_z - poster_width * 0.5f;
        float y0 = poster_center_y - poster_height * 0.5f;
        float y1 = poster_center_y + poster_height * 0.5f;

        std::string name = "poster_" + std::to_string(i);
        add_poster_quad(name,
                        glm::vec3(poster_x, y0, z_near),
                        glm::vec3(poster_x, y0, z_far),
                        glm::vec3(poster_x, y1, z_far),
                        glm::vec3(poster_x, y1, z_near),
                        glm::vec3(1.0f, 0.0f, 0.0f));
    }

    GLuint vertex_buffer_id;
    glGenBuffers(1, &vertex_buffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(PosterVertex), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(PosterVertex), (void*)offsetof(PosterVertex, px));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(PosterVertex), (void*)offsetof(PosterVertex, nx));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(PosterVertex), (void*)offsetof(PosterVertex, u));
    glEnableVertexAttribArray(2);

    GLuint index_buffer_id;
    glGenBuffers(1, &index_buffer_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

// Função que carrega os shaders de vértices e de fragmentos que serão
// utilizados para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
//
void LoadShadersFromFiles()
{
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
    GLuint fragment_shader_id = LoadShader_Fragment("../../src/shader_fragment.glsl");

    // Deletamos o programa de GPU anterior, caso ele exista.
    if ( g_GpuProgramID != 0 )
        glDeleteProgram(g_GpuProgramID);

    // Criamos um programa de GPU utilizando os shaders carregados acima.
    g_GpuProgramID = CreateGpuProgram(vertex_shader_id, fragment_shader_id);

    // Buscamos o endereço das variáveis definidas dentro do Vertex Shader.
    // Utilizaremos estas variáveis para enviar dados para a placa de vídeo
    // (GPU)! Veja arquivo "shader_vertex.glsl" e "shader_fragment.glsl".
    g_model_uniform      = glGetUniformLocation(g_GpuProgramID, "model"); // Variável da matriz "model"
    g_view_uniform       = glGetUniformLocation(g_GpuProgramID, "view"); // Variável da matriz "view" em shader_vertex.glsl
    g_projection_uniform = glGetUniformLocation(g_GpuProgramID, "projection"); // Variável da matriz "projection" em shader_vertex.glsl
    g_bbox_min_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_min");
    g_bbox_max_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_max");
    g_camera_position_uniform = glGetUniformLocation(g_GpuProgramID, "camera_position");
    g_material_diffuse_uniform = glGetUniformLocation(g_GpuProgramID, "material.diffuse_texture");
    g_material_specular_strength_uniform = glGetUniformLocation(g_GpuProgramID, "material.specular_strength");
    g_material_shininess_uniform = glGetUniformLocation(g_GpuProgramID, "material.shininess");
    g_material_ambient_strength_uniform = glGetUniformLocation(g_GpuProgramID, "material.ambient_strength");
    g_material_uv_scale_uniform = glGetUniformLocation(g_GpuProgramID, "material.uv_scale");
    g_num_lights_uniform = glGetUniformLocation(g_GpuProgramID, "num_lights");

    for (int i = 0; i < kMaxLights; ++i)
    {
        std::string prefix = "lights[" + std::to_string(i) + "].";
        g_light_position_uniforms[i] = glGetUniformLocation(g_GpuProgramID, (prefix + "position").c_str());
        g_light_color_uniforms[i] = glGetUniformLocation(g_GpuProgramID, (prefix + "color").c_str());
        g_light_ambient_strength_uniforms[i] = glGetUniformLocation(g_GpuProgramID, (prefix + "ambient_strength").c_str());
        g_light_diffuse_strength_uniforms[i] = glGetUniformLocation(g_GpuProgramID, (prefix + "diffuse_strength").c_str());
        g_light_specular_strength_uniforms[i] = glGetUniformLocation(g_GpuProgramID, (prefix + "specular_strength").c_str());
        g_light_constant_uniforms[i] = glGetUniformLocation(g_GpuProgramID, (prefix + "constant").c_str());
        g_light_linear_uniforms[i] = glGetUniformLocation(g_GpuProgramID, (prefix + "linear").c_str());
        g_light_quadratic_uniforms[i] = glGetUniformLocation(g_GpuProgramID, (prefix + "quadratic").c_str());
    }
}

// Função que pega a matriz M e guarda a mesma no topo da pilha
void PushMatrix(glm::mat4 M)
{
    g_MatrixStack.push(M);
}

// Função que remove a matriz atualmente no topo da pilha e armazena a mesma na variável M
void PopMatrix(glm::mat4& M)
{
    if ( g_MatrixStack.empty() )
    {
        M = Matrix_Identity();
    }
    else
    {
        M = g_MatrixStack.top();
        g_MatrixStack.pop();
    }
}

// Função que computa as normais de um ObjModel, caso elas não tenham sido
// especificadas dentro do arquivo ".obj"
void ComputeNormals(ObjModel* model)
{
    if ( !model->attrib.normals.empty() )
        return;

    // Primeiro computamos as normais para todos os TRIÂNGULOS.
    // Segundo, computamos as normais dos VÉRTICES através do método proposto
    // por Gouraud, onde a normal de cada vértice vai ser a média das normais de
    // todas as faces que compartilham este vértice e que pertencem ao mesmo "smoothing group".

    // Obtemos a lista dos smoothing groups que existem no objeto
    std::set<unsigned int> sgroup_ids;
    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        assert(model->shapes[shape].mesh.smoothing_group_ids.size() == num_triangles);

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);
            unsigned int sgroup = model->shapes[shape].mesh.smoothing_group_ids[triangle];
            assert(sgroup >= 0);
            sgroup_ids.insert(sgroup);
        }
    }

    size_t num_vertices = model->attrib.vertices.size() / 3;
    model->attrib.normals.reserve( 3*num_vertices );

    // Processamos um smoothing group por vez
    for (const unsigned int & sgroup : sgroup_ids)
    {
        std::vector<int> num_triangles_per_vertex(num_vertices, 0);
        std::vector<glm::vec4> vertex_normals(num_vertices, glm::vec4(0.0f,0.0f,0.0f,0.0f));

        // Acumulamos as normais dos vértices de todos triângulos deste smoothing group
        for (size_t shape = 0; shape < model->shapes.size(); ++shape)
        {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

            for (size_t triangle = 0; triangle < num_triangles; ++triangle)
            {
                unsigned int sgroup_tri = model->shapes[shape].mesh.smoothing_group_ids[triangle];

                if (sgroup_tri != sgroup)
                    continue;

                glm::vec4  vertices[3];
                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                    const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                    const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                    vertices[vertex] = glm::vec4(vx,vy,vz,1.0);
                }

                const glm::vec4  a = vertices[0];
                const glm::vec4  b = vertices[1];
                const glm::vec4  c = vertices[2];

                const glm::vec4  n = crossproduct(b-a,c-a);

                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    num_triangles_per_vertex[idx.vertex_index] += 1;
                    vertex_normals[idx.vertex_index] += n;
                }
            }
        }

        // Computamos a média das normais acumuladas
        std::vector<size_t> normal_indices(num_vertices, 0);

        for (size_t vertex_index = 0; vertex_index < vertex_normals.size(); ++vertex_index)
        {
            if (num_triangles_per_vertex[vertex_index] == 0)
                continue;

            glm::vec4 n = vertex_normals[vertex_index] / (float)num_triangles_per_vertex[vertex_index];
            n /= norm(n);

            model->attrib.normals.push_back( n.x );
            model->attrib.normals.push_back( n.y );
            model->attrib.normals.push_back( n.z );

            size_t normal_index = (model->attrib.normals.size() / 3) - 1;
            normal_indices[vertex_index] = normal_index;
        }

        // Escrevemos os índices das normais para os vértices dos triângulos deste smoothing group
        for (size_t shape = 0; shape < model->shapes.size(); ++shape)
        {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

            for (size_t triangle = 0; triangle < num_triangles; ++triangle)
            {
                unsigned int sgroup_tri = model->shapes[shape].mesh.smoothing_group_ids[triangle];

                if (sgroup_tri != sgroup)
                    continue;

                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    model->shapes[shape].mesh.indices[3*triangle + vertex].normal_index =
                        normal_indices[ idx.vertex_index ];
                }
            }
        }

    }
}

void BuildCorridorAndAddToVirtualScene()
{
    struct CorridorVertex
    {
        float px, py, pz, pw;
        float nx, ny, nz, nw;
        float u, v;
    };

    std::vector<CorridorVertex> vertices;
    std::vector<GLuint> indices;

    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    auto add_quad = [&](const std::string& name,
                        glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3,
                        glm::vec3 normal,
                        float u_max, float v_max)
    {
        size_t first_index = indices.size();
        GLuint base_vertex = static_cast<GLuint>(vertices.size());

        auto push_vertex = [&](glm::vec3 p, float u, float v)
        {
            CorridorVertex vertex;
            vertex.px = p.x; vertex.py = p.y; vertex.pz = p.z; vertex.pw = 1.0f;
            vertex.nx = normal.x; vertex.ny = normal.y; vertex.nz = normal.z; vertex.nw = 0.0f;
            vertex.u = u; vertex.v = v;
            vertices.push_back(vertex);
        };

        push_vertex(p0, 0.0f, 0.0f);
        push_vertex(p1, u_max, 0.0f);
        push_vertex(p2, u_max, v_max);
        push_vertex(p3, 0.0f, v_max);

        indices.push_back(base_vertex + 0);
        indices.push_back(base_vertex + 1);
        indices.push_back(base_vertex + 2);
        indices.push_back(base_vertex + 0);
        indices.push_back(base_vertex + 2);
        indices.push_back(base_vertex + 3);

        glm::vec3 bbox_min = p0;
        glm::vec3 bbox_max = p0;
        bbox_min.x = std::min(bbox_min.x, p1.x); bbox_min.y = std::min(bbox_min.y, p1.y); bbox_min.z = std::min(bbox_min.z, p1.z);
        bbox_max.x = std::max(bbox_max.x, p1.x); bbox_max.y = std::max(bbox_max.y, p1.y); bbox_max.z = std::max(bbox_max.z, p1.z);
        bbox_min.x = std::min(bbox_min.x, p2.x); bbox_min.y = std::min(bbox_min.y, p2.y); bbox_min.z = std::min(bbox_min.z, p2.z);
        bbox_max.x = std::max(bbox_max.x, p2.x); bbox_max.y = std::max(bbox_max.y, p2.y); bbox_max.z = std::max(bbox_max.z, p2.z);
        bbox_min.x = std::min(bbox_min.x, p3.x); bbox_min.y = std::min(bbox_min.y, p3.y); bbox_min.z = std::min(bbox_min.z, p3.z);
        bbox_max.x = std::max(bbox_max.x, p3.x); bbox_max.y = std::max(bbox_max.y, p3.y); bbox_max.z = std::max(bbox_max.z, p3.z);

        SceneObject object;
        object.name = name;
        object.first_index = first_index;
        object.num_indices = 6;
        object.rendering_mode = GL_TRIANGLES;
        object.vertex_array_object_id = vertex_array_object_id;
        object.bbox_min = bbox_min;
        object.bbox_max = bbox_max;
        g_VirtualScene[name] = object;
    };

    const float half_width = kCorridorHalfWidth;
    const float corridor_height = kCorridorHeight;
    const float corridor_length = kCorridorLength;
    const float z0 = kCorridorZ0;
    const float z1 = kCorridorZ1;

    add_quad("corridor_floor",
             glm::vec3(-half_width, 0.0f, z0),
             glm::vec3(+half_width, 0.0f, z0),
             glm::vec3(+half_width, 0.0f, z1),
             glm::vec3(-half_width, 0.0f, z1),
             glm::vec3(0.0f, 1.0f, 0.0f),
             1.0f, 1.0f);

    add_quad("corridor_ceiling",
             glm::vec3(-half_width, corridor_height, z1),
             glm::vec3(+half_width, corridor_height, z1),
             glm::vec3(+half_width, corridor_height, z0),
             glm::vec3(-half_width, corridor_height, z0),
             glm::vec3(0.0f, -1.0f, 0.0f),
             1.0f, 1.0f);

    add_quad("corridor_wall_left",
             glm::vec3(-half_width, 0.0f, z0),
             glm::vec3(-half_width, 0.0f, z1),
             glm::vec3(-half_width, corridor_height, z1),
             glm::vec3(-half_width, corridor_height, z0),
             glm::vec3(1.0f, 0.0f, 0.0f),
             1.0f, 1.0f);

    add_quad("corridor_wall_right",
             glm::vec3(+half_width, 0.0f, z1),
             glm::vec3(+half_width, 0.0f, z0),
             glm::vec3(+half_width, corridor_height, z0),
             glm::vec3(+half_width, corridor_height, z1),
             glm::vec3(-1.0f, 0.0f, 0.0f),
             1.0f, 1.0f);

    GLuint vertex_buffer_id;
    glGenBuffers(1, &vertex_buffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(CorridorVertex), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(CorridorVertex), (void*)offsetof(CorridorVertex, px));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(CorridorVertex), (void*)offsetof(CorridorVertex, nx));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(CorridorVertex), (void*)offsetof(CorridorVertex, u));
    glEnableVertexAttribArray(2);

    GLuint index_buffer_id;
    glGenBuffers(1, &index_buffer_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void BuildCornerAndAddToVirtualScene()
{
    struct CornerVertex { float px, py, pz, pw; float nx, ny, nz, nw; float u, v; };

    // Agora recebemos 4 booleanos, um para cada parede possível!
    auto build_corner_parts = [&](const std::string& prefix, bool wall_front, bool wall_back, bool wall_left, bool wall_right)
    {
        auto add_quad = [&](const std::string& name, glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 normal)
        {
            std::vector<CornerVertex> vertices;
            std::vector<GLuint> indices = {0, 1, 2, 0, 2, 3};

            vertices.push_back({p0.x, p0.y, p0.z, 1.0f, normal.x, normal.y, normal.z, 0.0f, 0.0f, 0.0f});
            vertices.push_back({p1.x, p1.y, p1.z, 1.0f, normal.x, normal.y, normal.z, 0.0f, 1.0f, 0.0f});
            vertices.push_back({p2.x, p2.y, p2.z, 1.0f, normal.x, normal.y, normal.z, 0.0f, 1.0f, 1.0f});
            vertices.push_back({p3.x, p3.y, p3.z, 1.0f, normal.x, normal.y, normal.z, 0.0f, 0.0f, 1.0f});

            GLuint vao, vbo, ebo;
            glGenVertexArrays(1, &vao); glBindVertexArray(vao);
            glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo); glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(CornerVertex), vertices.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(CornerVertex), (void*)offsetof(CornerVertex, px)); glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(CornerVertex), (void*)offsetof(CornerVertex, nx)); glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(CornerVertex), (void*)offsetof(CornerVertex, u)); glEnableVertexAttribArray(2);
            glGenBuffers(1, &ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
            glBindVertexArray(0);

            SceneObject object;
            object.name = name;
            object.first_index = 0;
            object.num_indices = 6;
            object.rendering_mode = GL_TRIANGLES;
            object.vertex_array_object_id = vao;
            object.bbox_min = p3; object.bbox_max = p1; 
            g_VirtualScene[name] = object;
        };

        const float hw = kCorridorHalfWidth;
        const float h = kCorridorHeight;
        const float z0 = 0.0f;
        const float z1 = -kCornerLength;

        // Chão e Teto
        add_quad(prefix + "_floor", glm::vec3(-hw, 0.0f, z0), glm::vec3(hw, 0.0f, z0), glm::vec3(hw, 0.0f, z1), glm::vec3(-hw, 0.0f, z1), glm::vec3(0.0f, 1.0f, 0.0f));
        add_quad(prefix + "_ceiling", glm::vec3(-hw, h, z1), glm::vec3(hw, h, z1), glm::vec3(hw, h, z0), glm::vec3(-hw, h, z0), glm::vec3(0.0f, -1.0f, 0.0f));
        
        // Paredes dinâmicas (só cria se for TRUE)
        if (wall_front) add_quad(prefix + "_wall_front", glm::vec3(hw, 0.0f, z0), glm::vec3(-hw, 0.0f, z0), glm::vec3(-hw, h, z0), glm::vec3(hw, h, z0), glm::vec3(0.0f, 0.0f, -1.0f));
        if (wall_back)  add_quad(prefix + "_wall_back", glm::vec3(-hw, 0.0f, z1), glm::vec3(hw, 0.0f, z1), glm::vec3(hw, h, z1), glm::vec3(-hw, h, z1), glm::vec3(0.0f, 0.0f, 1.0f));
        if (wall_left)  add_quad(prefix + "_wall_left", glm::vec3(-hw, 0.0f, z0), glm::vec3(-hw, 0.0f, z1), glm::vec3(-hw, h, z1), glm::vec3(-hw, h, z0), glm::vec3(1.0f, 0.0f, 0.0f));
        if (wall_right) add_quad(prefix + "_wall_right", glm::vec3(hw, 0.0f, z1), glm::vec3(hw, 0.0f, z0), glm::vec3(hw, h, z0), glm::vec3(hw, h, z1), glm::vec3(-1.0f, 0.0f, 0.0f));
    };

    // Quina 1 (Vira à esquerda): Aberta na frente (para o Corredor 1) e aberta na esquerda (para o Conector).
    // Tem parede no fundo e na direita.
    build_corner_parts("corner_left", false, true, false, true);

    // Quina 2 (Vira à direita): Aberta na direita (vindo do Conector) e aberta no fundo (para o Corredor 2).
    // Tem parede na frente e na esquerda.
    build_corner_parts("corner_right", true, false, true, false);
}

// Constrói triângulos para futura renderização a partir de um ObjModel.
void BuildTrianglesAndAddToVirtualScene(ObjModel* model)
{
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    std::vector<GLuint> indices;
    std::vector<float>  model_coefficients;
    std::vector<float>  normal_coefficients;
    std::vector<float>  texture_coefficients;

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t first_index = indices.size();
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        const float minval = std::numeric_limits<float>::min();
        const float maxval = std::numeric_limits<float>::max();

        glm::vec3 bbox_min = glm::vec3(maxval,maxval,maxval);
        glm::vec3 bbox_max = glm::vec3(minval,minval,minval);

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];

                indices.push_back(first_index + 3*triangle + vertex);

                const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                //printf("tri %d vert %d = (%.2f, %.2f, %.2f)\n", (int)triangle, (int)vertex, vx, vy, vz);
                model_coefficients.push_back( vx ); // X
                model_coefficients.push_back( vy ); // Y
                model_coefficients.push_back( vz ); // Z
                model_coefficients.push_back( 1.0f ); // W

                bbox_min.x = std::min(bbox_min.x, vx);
                bbox_min.y = std::min(bbox_min.y, vy);
                bbox_min.z = std::min(bbox_min.z, vz);
                bbox_max.x = std::max(bbox_max.x, vx);
                bbox_max.y = std::max(bbox_max.y, vy);
                bbox_max.z = std::max(bbox_max.z, vz);

                // Inspecionando o código da tinyobjloader, o aluno Bernardo
                // Sulzbach (2017/1) apontou que a maneira correta de testar se
                // existem normais e coordenadas de textura no ObjModel é
                // comparando se o índice retornado é -1. Fazemos isso abaixo.

                if ( idx.normal_index != -1 )
                {
                    const float nx = model->attrib.normals[3*idx.normal_index + 0];
                    const float ny = model->attrib.normals[3*idx.normal_index + 1];
                    const float nz = model->attrib.normals[3*idx.normal_index + 2];
                    normal_coefficients.push_back( nx ); // X
                    normal_coefficients.push_back( ny ); // Y
                    normal_coefficients.push_back( nz ); // Z
                    normal_coefficients.push_back( 0.0f ); // W
                }

                if ( idx.texcoord_index != -1 )
                {
                    const float u = model->attrib.texcoords[2*idx.texcoord_index + 0];
                    const float v = model->attrib.texcoords[2*idx.texcoord_index + 1];
                    texture_coefficients.push_back( u );
                    texture_coefficients.push_back( v );
                }
            }
        }

        size_t last_index = indices.size() - 1;

        SceneObject theobject;
        theobject.name           = model->shapes[shape].name;
        theobject.first_index    = first_index; // Primeiro índice
        theobject.num_indices    = last_index - first_index + 1; // Número de indices
        theobject.rendering_mode = GL_TRIANGLES;       // Índices correspondem ao tipo de rasterização GL_TRIANGLES.
        theobject.vertex_array_object_id = vertex_array_object_id;

        theobject.bbox_min = bbox_min;
        theobject.bbox_max = bbox_max;

        g_VirtualScene[model->shapes[shape].name] = theobject;
    }

    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, model_coefficients.size() * sizeof(float), model_coefficients.data());
    GLuint location = 0; // "(location = 0)" em "shader_vertex.glsl"
    GLint  number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if ( !normal_coefficients.empty() )
    {
        GLuint VBO_normal_coefficients_id;
        glGenBuffers(1, &VBO_normal_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, normal_coefficients.size() * sizeof(float), normal_coefficients.data());
        location = 1; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if ( !texture_coefficients.empty() )
    {
        GLuint VBO_texture_coefficients_id;
        glGenBuffers(1, &VBO_texture_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, texture_coefficients.size() * sizeof(float), texture_coefficients.data());
        location = 2; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 2; // vec2 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    GLuint indices_id;
    glGenBuffers(1, &indices_id);

    // "Ligamos" o buffer. Note que o tipo agora é GL_ELEMENT_ARRAY_BUFFER.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(GLuint), indices.data());
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // XXX Errado!
    //

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

// Carrega um Vertex Shader de um arquivo GLSL. Veja definição de LoadShader() abaixo.
GLuint LoadShader_Vertex(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos vértices.
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, vertex_shader_id);

    // Retorna o ID gerado acima
    return vertex_shader_id;
}

// Carrega um Fragment Shader de um arquivo GLSL . Veja definição de LoadShader() abaixo.
GLuint LoadShader_Fragment(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos fragmentos.
    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, fragment_shader_id);

    // Retorna o ID gerado acima
    return fragment_shader_id;
}

// Função auxilar, utilizada pelas duas funções acima. Carrega código de GPU de
// um arquivo GLSL e faz sua compilação.
void LoadShader(const char* filename, GLuint shader_id)
{
    // Lemos o arquivo de texto indicado pela variável "filename"
    // e colocamos seu conteúdo em memória, apontado pela variável
    // "shader_string".
    std::ifstream file;
    try {
        file.exceptions(std::ifstream::failbit);
        file.open(filename);
    } catch ( std::exception& e ) {
        fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }
    std::stringstream shader;
    shader << file.rdbuf();
    std::string str = shader.str();
    const GLchar* shader_string = str.c_str();
    const GLint   shader_string_length = static_cast<GLint>( str.length() );

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
    GLchar* log = new GLchar[log_length];
    glGetShaderInfoLog(shader_id, log_length, &log_length, log);

    // Imprime no terminal qualquer erro ou "warning" de compilação
    if ( log_length != 0 )
    {
        std::string  output;

        if ( !compiled_ok )
        {
            output += "ERROR: OpenGL compilation of \"";
            output += filename;
            output += "\" failed.\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }
        else
        {
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
    delete [] log;
}

// Esta função cria um programa de GPU, o qual contém obrigatoriamente um
// Vertex Shader e um Fragment Shader.
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id)
{
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
    if ( linked_ok == GL_FALSE )
    {
        GLint log_length = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

        // Alocamos memória para guardar o log de compilação.
        // A chamada "new" em C++ é equivalente ao "malloc()" do C.
        GLchar* log = new GLchar[log_length];

        glGetProgramInfoLog(program_id, log_length, &log_length, log);

        std::string output;

        output += "ERROR: OpenGL linking of program failed.\n";
        output += "== Start of link log\n";
        output += log;
        output += "\n== End of link log\n";

        // A chamada "delete" em C++ é equivalente ao "free()" do C
        delete [] log;

        fprintf(stderr, "%s", output.c_str());
    }

    // Os "Shader Objects" podem ser marcados para deleção após serem linkados 
    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    // Retornamos o ID gerado acima
    return program_id;
}

glm::vec4 ComputeCameraFrontVector()
{
    float cos_pitch = cos(g_CameraPitch);
    glm::vec4 front = glm::vec4(
        cos_pitch * sin(g_CameraYaw),
        sin(g_CameraPitch),
        -cos_pitch * cos(g_CameraYaw),
        0.0f
    );
    return front / norm(front);
}

void UpdateCameraFromInput(GLFWwindow* window, float delta_time)
{
    float movement_speed = 4.0f;
    float step = movement_speed * delta_time;

    glm::vec4 front = ComputeCameraFrontVector();
    glm::vec4 world_up = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    glm::vec4 right = crossproduct(front, world_up);
    right = right / norm(right);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) g_CameraPosition += front * step;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) g_CameraPosition -= front * step;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) g_CameraPosition -= right * step;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) g_CameraPosition += right * step;

    g_CameraPosition.y = 1.6f;

    struct WalkableBox2D
    {
        float min_x;
        float max_x;
        float min_z;
        float max_z;
    };

    const float player_radius = 0.15f;
    const float first_x_limit = kCorridorHalfWidth - player_radius;
    const float first_z_max = 0.8f;

    const float corridor2_offset_x = -(2.0f * kCorridorHalfWidth + kConnectorLength);
    const float second_corridor_z_offset = kCorridorZ1 - kCornerLength; // -44.0f (deslocamento entre blocos)
    const glm::vec2 block_offset(corridor2_offset_x, second_corridor_z_offset);
    const float connector_center_z = kCorridorZ1 - 0.5f * kCornerLength;
    const float connector_half_width = kCorridorHalfWidth - player_radius;

    // Todas as caixas abaixo estão no espaço local do "Bloco 0" (tile central).
    const WalkableBox2D corridor1 = {-first_x_limit, +first_x_limit, kCorridorZ1, first_z_max};
    const WalkableBox2D corner_left = {-kCorridorHalfWidth, +kCorridorHalfWidth,
                                       kCorridorZ1 - kCornerLength + player_radius, kCorridorZ1};
    const WalkableBox2D connector = {-kCorridorHalfWidth - kConnectorLength, -kCorridorHalfWidth,
                                     connector_center_z - connector_half_width, connector_center_z + connector_half_width};
    const WalkableBox2D corner_right = {corridor2_offset_x - kCorridorHalfWidth, corridor2_offset_x + kCorridorHalfWidth,
                                        kCorridorZ1 - kCornerLength + player_radius, kCorridorZ1};

    auto clampf = [](float value, float min_value, float max_value)
    {
        return std::max(min_value, std::min(max_value, value));
    };

    auto inside_box = [](const WalkableBox2D& box, float x, float z)
    {
        return x >= box.min_x && x <= box.max_x && z >= box.min_z && z <= box.max_z;
    };

    auto closest_point = [&](const WalkableBox2D& box, glm::vec2 p)
    {
        return glm::vec2(clampf(p.x, box.min_x, box.max_x),
                         clampf(p.y, box.min_z, box.max_z));
    };

    glm::vec2 p_world(g_CameraPosition.x, g_CameraPosition.z);
    glm::vec2 p = p_world;

    // (7) Collision wrapping: mapeia a posição do mundo para o espaço local do Bloco 0
    // usando o mesmo deslocamento geométrico entre tiles.
    int block_index = 0;
    const float wrap_min_z = kCorridorZ1 - kCornerLength + player_radius; // menor z caminhável dentro de um bloco
    const float wrap_max_z = first_z_max;                                 // maior z caminhável dentro de um bloco
    while (p.y < wrap_min_z) { p -= block_offset; ++block_index; } // veio do Bloco +1 (à frente)
    while (p.y > wrap_max_z) { p += block_offset; --block_index; } // veio do Bloco -1 (atrás)

    if (!inside_box(corridor1, p.x, p.y) &&
        !inside_box(corner_left, p.x, p.y) &&
        !inside_box(connector, p.x, p.y) &&
        !inside_box(corner_right, p.x, p.y))
    {
        glm::vec2 best = closest_point(corridor1, p);
        float best_dist2 = (best.x - p.x) * (best.x - p.x) + (best.y - p.y) * (best.y - p.y);

        glm::vec2 candidate = closest_point(corner_left, p);
        float dist2 = (candidate.x - p.x) * (candidate.x - p.x) + (candidate.y - p.y) * (candidate.y - p.y);
        if (dist2 < best_dist2)
        {
            best = candidate;
            best_dist2 = dist2;
        }

        candidate = closest_point(connector, p);
        dist2 = (candidate.x - p.x) * (candidate.x - p.x) + (candidate.y - p.y) * (candidate.y - p.y);
        if (dist2 < best_dist2)
        {
            best = candidate;
            best_dist2 = dist2;
        }

        candidate = closest_point(corner_right, p);
        dist2 = (candidate.x - p.x) * (candidate.x - p.x) + (candidate.y - p.y) * (candidate.y - p.y);
        if (dist2 < best_dist2)
        {
            best = candidate;
            best_dist2 = dist2;
        }

        p = best;
    }

    // (5-6) Bi-directional treadmill recentering: mantém o jogador perto do Bloco 0.
    // Quando ele avança "demais" no Bloco +1, subtrai o offset do bloco;
    // quando ele recua "demais" no Bloco -1, soma o offset.
    const float recenter_plane_z = 0.5f * kCorridorZ1; // -20.0f (meio do corredor reto)
    p_world = p + (float)block_index * block_offset;
    while (block_index > 0 && p.y < recenter_plane_z)
    {
        p_world -= block_offset; // traz do Bloco +1 para o Bloco 0 (subtrai (corridor2_offset_x, second_corridor_z_offset))
        --block_index;
    }
    while (block_index < 0 && p.y > recenter_plane_z)
    {
        p_world += block_offset; // traz do Bloco -1 para o Bloco 0 (soma (corridor2_offset_x, second_corridor_z_offset))
        ++block_index;
    }

    g_CameraPosition.x = p_world.x;
    g_CameraPosition.z = p_world.y;
    g_CameraPosition.w = 1.0f;
}

// Definição da função que será chamada sempre que a janela do sistema
// operacional for redimensionada, por consequência alterando o tamanho do
// "framebuffer" (região de memória onde são armazenados os pixels da imagem).
void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    // Indicamos que queremos renderizar em toda região do framebuffer. A
    // função "glViewport" define o mapeamento das "normalized device
    // coordinates" (NDC) para "pixel coordinates".  Essa é a operação de
    // "Screen Mapping" ou "Viewport Mapping" vista em aula ({+ViewportMapping2+}).
    glViewport(0, 0, width, height);

    // Atualizamos também a razão que define a proporção da janela (largura /
    // altura), a qual será utilizada na definição das matrizes de projeção,
    // tal que não ocorra distorções durante o processo de "Screen Mapping"
    // acima, quando NDC é mapeado para coordenadas de pixels. Veja slides 205-215 do documento Aula_09_Projecoes.pdf.
    //
    // O cast para float é necessário pois números inteiros são arredondados ao
    // serem divididos!
    g_ScreenRatio = (float)width / height;
}

// Variáveis globais que armazenam a última posição do cursor do mouse, para
// que possamos calcular quanto que o mouse se movimentou entre dois instantes
// de tempo. Utilizadas no callback CursorPosCallback() abaixo.
double g_LastCursorPosX, g_LastCursorPosY;

// Função callback chamada sempre que o usuário aperta algum dos botões do mouse
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_LeftMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_LeftMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_LeftMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_RightMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_RightMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_RightMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_MiddleMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_MiddleMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_MiddleMouseButtonPressed = false;
    }
}

// Função callback chamada sempre que o usuário movimentar o cursor do mouse em
// cima da janela OpenGL.
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    (void)window;
    if (g_FirstMouseInput)
    {
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
    if (g_CameraPitch > pitch_limit) g_CameraPitch = pitch_limit;
    if (g_CameraPitch < -pitch_limit) g_CameraPitch = -pitch_limit;
}

// Função callback chamada sempre que o usuário movimenta a "rodinha" do mouse.
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    (void)window;
    (void)xoffset;
    (void)yoffset;
}

void Correcao_KeyCallback(int key, int action, int mod);

// Definição da função que será chamada sempre que o usuário pressionar alguma
// tecla do teclado. Veja http://www.glfw.org/docs/latest/input_guide.html#input_key
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mod)
{
    // =======================
    // Não modifique esta chamada! Ela é utilizada para correção automatizada dos
    // laboratórios. Deve ser sempre o primeiro comando desta função KeyCallback().
    Correcao_KeyCallback(key, action, mod);
    // =======================

    // Se o usuário pressionar a tecla ESC, fechamos a janela.
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    // O código abaixo implementa a seguinte lógica:
    //   Se apertar tecla X       então g_AngleX += delta;
    //   Se apertar tecla shift+X então g_AngleX -= delta;
    //   Se apertar tecla Y       então g_AngleY += delta;
    //   Se apertar tecla shift+Y então g_AngleY -= delta;
    //   Se apertar tecla Z       então g_AngleZ += delta;
    //   Se apertar tecla shift+Z então g_AngleZ -= delta;

    float delta = 3.141592 / 16; // 22.5 graus, em radianos.

    if (key == GLFW_KEY_X && action == GLFW_PRESS)
    {
        g_AngleX += (mod & GLFW_MOD_SHIFT) ? -delta : delta;
    }

    if (key == GLFW_KEY_Y && action == GLFW_PRESS)
    {
        g_AngleY += (mod & GLFW_MOD_SHIFT) ? -delta : delta;
    }
    if (key == GLFW_KEY_Z && action == GLFW_PRESS)
    {
        g_AngleZ += (mod & GLFW_MOD_SHIFT) ? -delta : delta;
    }

    // Se o usuário apertar a tecla espaço, resetamos os ângulos de Euler para zero.
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        g_AngleX = 0.0f;
        g_AngleY = 0.0f;
        g_AngleZ = 0.0f;
        g_ForearmAngleX = 0.0f;
        g_ForearmAngleZ = 0.0f;
        g_TorsoPositionX = 0.0f;
        g_TorsoPositionY = 0.0f;
    }

    // Se o usuário apertar a tecla P, utilizamos projeção perspectiva.
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = true;
    }

    // Se o usuário apertar a tecla O, utilizamos projeção ortográfica.
    if (key == GLFW_KEY_O && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = false;
    }

    // Se o usuário apertar a tecla H, fazemos um "toggle" do texto informativo mostrado na tela.
    if (key == GLFW_KEY_H && action == GLFW_PRESS)
    {
        g_ShowInfoText = !g_ShowInfoText;
    }

    // Se o usuário apertar a tecla R, recarregamos os shaders dos arquivos "shader_fragment.glsl" e "shader_vertex.glsl".
    if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        LoadShadersFromFiles();
        fprintf(stdout,"Shaders recarregados!\n");
        fflush(stdout);
    }
}

// Definimos o callback para impressão de erros da GLFW no terminal
void ErrorCallback(int error, const char* description)
{
    fprintf(stderr, "ERROR: GLFW: %s\n", description);
}

// Esta função recebe um vértice com coordenadas de modelo p_model e passa o
// mesmo por todos os sistemas de coordenadas armazenados nas matrizes model,
// view, e projection; e escreve na tela as matrizes e pontos resultantes
// dessas transformações.
void TextRendering_ShowModelViewProjection(
    GLFWwindow* window,
    glm::mat4 projection,
    glm::mat4 view,
    glm::mat4 model,
    glm::vec4 p_model
)
{
    if ( !g_ShowInfoText )
        return;

    glm::vec4 p_world = model*p_model;
    glm::vec4 p_camera = view*p_world;
    glm::vec4 p_clip = projection*p_camera;
    glm::vec4 p_ndc = p_clip / p_clip.w;

    float pad = TextRendering_LineHeight(window);

    TextRendering_PrintString(window, " Model matrix             Model     In World Coords.", -1.0f, 1.0f-pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, model, p_model, -1.0f, 1.0f-2*pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f-6*pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f-7*pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f-8*pad, 1.0f);

    TextRendering_PrintString(window, " View matrix              World     In Camera Coords.", -1.0f, 1.0f-9*pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, view, p_world, -1.0f, 1.0f-10*pad, 1.0f);

    TextRendering_PrintString(window, "                                        |  ", -1.0f, 1.0f-14*pad, 1.0f);
    TextRendering_PrintString(window, "                            .-----------'  ", -1.0f, 1.0f-15*pad, 1.0f);
    TextRendering_PrintString(window, "                            V              ", -1.0f, 1.0f-16*pad, 1.0f);

    TextRendering_PrintString(window, " Projection matrix        Camera                    In NDC", -1.0f, 1.0f-17*pad, 1.0f);
    TextRendering_PrintMatrixVectorProductDivW(window, projection, p_camera, -1.0f, 1.0f-18*pad, 1.0f);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    glm::vec2 a = glm::vec2(-1, -1);
    glm::vec2 b = glm::vec2(+1, +1);
    glm::vec2 p = glm::vec2( 0,  0);
    glm::vec2 q = glm::vec2(width, height);

    glm::mat4 viewport_mapping = Matrix(
        (q.x - p.x)/(b.x-a.x), 0.0f, 0.0f, (b.x*p.x - a.x*q.x)/(b.x-a.x),
        0.0f, (q.y - p.y)/(b.y-a.y), 0.0f, (b.y*p.y - a.y*q.y)/(b.y-a.y),
        0.0f , 0.0f , 1.0f , 0.0f ,
        0.0f , 0.0f , 0.0f , 1.0f
    );

    TextRendering_PrintString(window, "                                                       |  ", -1.0f, 1.0f-22*pad, 1.0f);
    TextRendering_PrintString(window, "                            .--------------------------'  ", -1.0f, 1.0f-23*pad, 1.0f);
    TextRendering_PrintString(window, "                            V                           ", -1.0f, 1.0f-24*pad, 1.0f);

    TextRendering_PrintString(window, " Viewport matrix           NDC      In Pixel Coords.", -1.0f, 1.0f-25*pad, 1.0f);
    TextRendering_PrintMatrixVectorProductMoreDigits(window, viewport_mapping, p_ndc, -1.0f, 1.0f-26*pad, 1.0f);
}

// Escrevemos na tela os ângulos de Euler definidos nas variáveis globais
// g_AngleX, g_AngleY, e g_AngleZ.
void TextRendering_ShowEulerAngles(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    float pad = TextRendering_LineHeight(window);

    char buffer[80];
    snprintf(buffer, 80, "Euler Angles rotation matrix = Z(%.2f)*Y(%.2f)*X(%.2f)\n", g_AngleZ, g_AngleY, g_AngleX);

    TextRendering_PrintString(window, buffer, -1.0f+pad/10, -1.0f+2*pad/10, 1.0f);
}

// Escrevemos na tela qual matriz de projeção está sendo utilizada.
void TextRendering_ShowProjection(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    if ( g_UsePerspectiveProjection )
        TextRendering_PrintString(window, "Perspective", 1.0f-13*charwidth, -1.0f+2*lineheight/10, 1.0f);
    else
        TextRendering_PrintString(window, "Orthographic", 1.0f-13*charwidth, -1.0f+2*lineheight/10, 1.0f);
}

// Escrevemos na tela o número de quadros renderizados por segundo (frames per
// second).
void TextRendering_ShowFramesPerSecond(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    // Variáveis estáticas (static) mantém seus valores entre chamadas
    // subsequentes da função!
    static float old_seconds = (float)glfwGetTime();
    static int   ellapsed_frames = 0;
    static char  buffer[20] = "?? fps";
    static int   numchars = 7;

    ellapsed_frames += 1;

    // Recuperamos o número de segundos que passou desde a execução do programa
    float seconds = (float)glfwGetTime();

    // Número de segundos desde o último cálculo do fps
    float ellapsed_seconds = seconds - old_seconds;

    if ( ellapsed_seconds > 1.0f )
    {
        numchars = snprintf(buffer, 20, "%.2f fps", ellapsed_frames / ellapsed_seconds);
    
        old_seconds = seconds;
        ellapsed_frames = 0;
    }

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth, 1.0f-lineheight, 1.0f);
}

// Função para debugging: imprime no terminal todas informações de um modelo
// geométrico carregado de um arquivo ".obj".
// Veja: https://github.com/syoyo/tinyobjloader/blob/22883def8db9ef1f3ffb9b404318e7dd25fdbb51/loader_example.cc#L98
void PrintObjModelInfo(ObjModel* model)
{
  const tinyobj::attrib_t                & attrib    = model->attrib;
  const std::vector<tinyobj::shape_t>    & shapes    = model->shapes;
  const std::vector<tinyobj::material_t> & materials = model->materials;

  printf("# of vertices  : %d\n", (int)(attrib.vertices.size() / 3));
  printf("# of normals   : %d\n", (int)(attrib.normals.size() / 3));
  printf("# of texcoords : %d\n", (int)(attrib.texcoords.size() / 2));
  printf("# of shapes    : %d\n", (int)shapes.size());
  printf("# of materials : %d\n", (int)materials.size());

  for (size_t v = 0; v < attrib.vertices.size() / 3; v++) {
    printf("  v[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.vertices[3 * v + 0]),
           static_cast<const double>(attrib.vertices[3 * v + 1]),
           static_cast<const double>(attrib.vertices[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.normals.size() / 3; v++) {
    printf("  n[%ld] = (%f, %f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.normals[3 * v + 0]),
           static_cast<const double>(attrib.normals[3 * v + 1]),
           static_cast<const double>(attrib.normals[3 * v + 2]));
  }

  for (size_t v = 0; v < attrib.texcoords.size() / 2; v++) {
    printf("  uv[%ld] = (%f, %f)\n", static_cast<long>(v),
           static_cast<const double>(attrib.texcoords[2 * v + 0]),
           static_cast<const double>(attrib.texcoords[2 * v + 1]));
  }

  // For each shape
  for (size_t i = 0; i < shapes.size(); i++) {
    printf("shape[%ld].name = %s\n", static_cast<long>(i),
           shapes[i].name.c_str());
    printf("Size of shape[%ld].indices: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.indices.size()));

    size_t index_offset = 0;

    assert(shapes[i].mesh.num_face_vertices.size() ==
           shapes[i].mesh.material_ids.size());

    printf("shape[%ld].num_faces: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.num_face_vertices.size()));

    // For each face
    for (size_t f = 0; f < shapes[i].mesh.num_face_vertices.size(); f++) {
      size_t fnum = shapes[i].mesh.num_face_vertices[f];

      printf("  face[%ld].fnum = %ld\n", static_cast<long>(f),
             static_cast<unsigned long>(fnum));

      // For each vertex in the face
      for (size_t v = 0; v < fnum; v++) {
        tinyobj::index_t idx = shapes[i].mesh.indices[index_offset + v];
        printf("    face[%ld].v[%ld].idx = %d/%d/%d\n", static_cast<long>(f),
               static_cast<long>(v), idx.vertex_index, idx.normal_index,
               idx.texcoord_index);
      }

      printf("  face[%ld].material_id = %d\n", static_cast<long>(f),
             shapes[i].mesh.material_ids[f]);

      index_offset += fnum;
    }

    printf("shape[%ld].num_tags: %lu\n", static_cast<long>(i),
           static_cast<unsigned long>(shapes[i].mesh.tags.size()));
    for (size_t t = 0; t < shapes[i].mesh.tags.size(); t++) {
      printf("  tag[%ld] = %s ", static_cast<long>(t),
             shapes[i].mesh.tags[t].name.c_str());
      printf(" ints: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].intValues.size(); ++j) {
        printf("%ld", static_cast<long>(shapes[i].mesh.tags[t].intValues[j]));
        if (j < (shapes[i].mesh.tags[t].intValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" floats: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].floatValues.size(); ++j) {
        printf("%f", static_cast<const double>(
                         shapes[i].mesh.tags[t].floatValues[j]));
        if (j < (shapes[i].mesh.tags[t].floatValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");

      printf(" strings: [");
      for (size_t j = 0; j < shapes[i].mesh.tags[t].stringValues.size(); ++j) {
        printf("%s", shapes[i].mesh.tags[t].stringValues[j].c_str());
        if (j < (shapes[i].mesh.tags[t].stringValues.size() - 1)) {
          printf(", ");
        }
      }
      printf("]");
      printf("\n");
    }
  }

  for (size_t i = 0; i < materials.size(); i++) {
    printf("material[%ld].name = %s\n", static_cast<long>(i),
           materials[i].name.c_str());
    printf("  material.Ka = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].ambient[0]),
           static_cast<const double>(materials[i].ambient[1]),
           static_cast<const double>(materials[i].ambient[2]));
    printf("  material.Kd = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].diffuse[0]),
           static_cast<const double>(materials[i].diffuse[1]),
           static_cast<const double>(materials[i].diffuse[2]));
    printf("  material.Ks = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].specular[0]),
           static_cast<const double>(materials[i].specular[1]),
           static_cast<const double>(materials[i].specular[2]));
    printf("  material.Tr = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].transmittance[0]),
           static_cast<const double>(materials[i].transmittance[1]),
           static_cast<const double>(materials[i].transmittance[2]));
    printf("  material.Ke = (%f, %f ,%f)\n",
           static_cast<const double>(materials[i].emission[0]),
           static_cast<const double>(materials[i].emission[1]),
           static_cast<const double>(materials[i].emission[2]));
    printf("  material.Ns = %f\n",
           static_cast<const double>(materials[i].shininess));
    printf("  material.Ni = %f\n", static_cast<const double>(materials[i].ior));
    printf("  material.dissolve = %f\n",
           static_cast<const double>(materials[i].dissolve));
    printf("  material.illum = %d\n", materials[i].illum);
    printf("  material.map_Ka = %s\n", materials[i].ambient_texname.c_str());
    printf("  material.map_Kd = %s\n", materials[i].diffuse_texname.c_str());
    printf("  material.map_Ks = %s\n", materials[i].specular_texname.c_str());
    printf("  material.map_Ns = %s\n",
           materials[i].specular_highlight_texname.c_str());
    printf("  material.map_bump = %s\n", materials[i].bump_texname.c_str());
    printf("  material.map_d = %s\n", materials[i].alpha_texname.c_str());
    printf("  material.disp = %s\n", materials[i].displacement_texname.c_str());
    printf("  <<PBR>>\n");
    printf("  material.Pr     = %f\n", materials[i].roughness);
    printf("  material.Pm     = %f\n", materials[i].metallic);
    printf("  material.Ps     = %f\n", materials[i].sheen);
    printf("  material.Pc     = %f\n", materials[i].clearcoat_thickness);
    printf("  material.Pcr    = %f\n", materials[i].clearcoat_thickness);
    printf("  material.aniso  = %f\n", materials[i].anisotropy);
    printf("  material.anisor = %f\n", materials[i].anisotropy_rotation);
    printf("  material.map_Ke = %s\n", materials[i].emissive_texname.c_str());
    printf("  material.map_Pr = %s\n", materials[i].roughness_texname.c_str());
    printf("  material.map_Pm = %s\n", materials[i].metallic_texname.c_str());
    printf("  material.map_Ps = %s\n", materials[i].sheen_texname.c_str());
    printf("  material.norm   = %s\n", materials[i].normal_texname.c_str());
    std::map<std::string, std::string>::const_iterator it(
        materials[i].unknown_parameter.begin());
    std::map<std::string, std::string>::const_iterator itEnd(
        materials[i].unknown_parameter.end());

    for (; it != itEnd; it++) {
      printf("  material.%s = %s\n", it->first.c_str(), it->second.c_str());
    }
    printf("\n");
  }
}

// set makeprg=cd\ ..\ &&\ make\ run\ >/dev/null
// vim: set spell spelllang=pt_br :

