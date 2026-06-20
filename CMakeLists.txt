# Este arquivo CMakeLists.txt foi adaptado a partir do projeto castor
# do PET INF/UFRGS (https://github.com/petcomputacaoufrgs/castor-fcg),
# com algumas modificações vindas do arquivo CMakeLists.txt criado
# pelos alunos Luis Melo e Santiago Gonzaga em 2023/1.

# Arquivos fonte C/C++. Inclua nesta lista todos os arquivos que devem
# ser compilados.
set(SOURCES
  src/main.cpp
  src/engine/Application.cpp
  src/engine/Audio.cpp
  src/engine/Camera.cpp
  src/engine/DebugText.cpp
  src/engine/Renderer.cpp
  src/engine/Shader.cpp
  src/engine/Texture.cpp
  src/entities/AnimatedModel.cpp
  src/entities/CamouflagedPursuer.cpp
  src/entities/NPC.cpp
  src/entities/Player.cpp
  src/rendering/Mesh.cpp
  src/rendering/WorldRenderer.cpp
  src/utils/FileUtils.cpp
  src/world/Corridor.cpp
  src/world/TeleportSystem.cpp
  src/correcao.cpp
  src/textrendering.cpp
  src/tiny_obj_loader.cpp
  src/stb_image.cpp
  src/glad.c
  src/collisions.cpp
)

cmake_minimum_required(VERSION 3.10.0)

project(LAB_FCG VERSION 1.0.0)

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

if(WIN32)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${PROJECT_SOURCE_DIR}/bin/Debug")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_SOURCE_DIR}/bin/Release")
elseif(UNIX)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/Linux")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
message(STATUS
  "Build type: ${CMAKE_BUILD_TYPE}

               Change the build type on the command line with

                   -DCMAKE_BUILD_TYPE=type

               for type in {Release, Debug, RelWithDebInfo}.
")

set(EXECUTABLE_NAME main)

# Verifica se todos os arquivos fonte estão presentes no diretório
# atual. Se não estão, avisa sobre CMakeLists mal configurado.
foreach(source_file IN LISTS SOURCES)
  if(NOT EXISTS ${PROJECT_SOURCE_DIR}/${source_file})
    message(FATAL_ERROR "
O arquivo ${PROJECT_SOURCE_DIR}/${source_file} não existe.
Por favor, atualize a lista de arquivos fonte no arquivo CMakeLists.txt.")
    break()
  endif()
endforeach()

add_executable(${EXECUTABLE_NAME} ${SOURCES})

target_include_directories(${EXECUTABLE_NAME} BEFORE PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/third_party/miniaudio
)

set(USE_BUNDLED_MSYS2_ASSIMP OFF)
if(WIN32 AND MINGW)
  set(LOCAL_MSYS2_UCRT_PREFIX
      "${PROJECT_SOURCE_DIR}/third_party/msys2-ucrt64/ucrt64")
  if(EXISTS
      "${LOCAL_MSYS2_UCRT_PREFIX}/lib/cmake/assimp-6.0/assimpConfig.cmake")
    set(CMAKE_PREFIX_PATH
        "${LOCAL_MSYS2_UCRT_PREFIX};${CMAKE_PREFIX_PATH}")
    set(USE_BUNDLED_MSYS2_ASSIMP ON)
  endif()
endif()
find_package(assimp CONFIG REQUIRED)
target_link_libraries(${EXECUTABLE_NAME} assimp::assimp)
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE SALARYMAN_HAS_ASSIMP=1)

if(WIN32)

  if(MINGW)

    # Aqui tentamos descobrir qual libc do Widows está sendo usada
    # pelo compilador MinGW: msvcrt (antiga) ou ucrt (nova). Também
    # diferenciamos entre um compilador 32-bits (antigo) ou 64-bits.
    # Para isso, buscamos pela ocorrência de algumas strings
    # específicas no output do comando "-v" do GCC, que lista os
    # parâmetros de configuração do compilador.
    # TODO: Testar com compilador llvm/clang.
    execute_process(
      COMMAND ${CMAKE_CXX_COMPILER} "-v"
      ERROR_VARIABLE  COMPILER_VERSION_OUTPUT
      RESULT_VARIABLE COMPILER_VERSION_RESULT
    )

    if (COMPILER_VERSION_RESULT EQUAL 0)
      # NOTE: É importante que o primeiro teste seja buscando pela
      # string ucrt64 no output do compilador, pois a string "mingw64"
      # sempre aparece no output (mesmo quando ucrt64 é a libc utilizada).
      if (COMPILER_VERSION_OUTPUT MATCHES "ucrt64")
        set(LIBGLFW ${PROJECT_SOURCE_DIR}/lib-ucrt-64/libglfw3.a)
      elseif (COMPILER_VERSION_OUTPUT MATCHES "mingw64")
        set(LIBGLFW ${PROJECT_SOURCE_DIR}/lib-mingw-64/libglfw3.a)
      else()
        set(LIBGLFW ${PROJECT_SOURCE_DIR}/lib-mingw-32/libglfw3.a)
      endif()
    else()
      message(FATAL_ERROR "Failed to get MinGW compiler version.")
    endif()

  elseif(MSVC)
    set(LIBGLFW ${PROJECT_SOURCE_DIR}/lib-vc2022/glfw3.lib)
  else()
    message(FATAL_ERROR "This CMakeLists.txt file only supports MINGW or MSVC toolchain on Windows.")
  endif()

  message(STATUS "LIBGLFW = ${LIBGLFW}")

  target_link_libraries(${EXECUTABLE_NAME} ${LIBGLFW} gdi32 opengl32)

  if(MINGW AND USE_BUNDLED_MSYS2_ASSIMP)
    set(BUNDLED_ASSIMP_RUNTIME_DLLS
      "${LOCAL_MSYS2_UCRT_PREFIX}/bin/libassimp-6.dll"
      "${LOCAL_MSYS2_UCRT_PREFIX}/bin/libminizip-1.dll"
      "${LOCAL_MSYS2_UCRT_PREFIX}/bin/zlib1.dll"
    )

    foreach(runtime_dll IN LISTS BUNDLED_ASSIMP_RUNTIME_DLLS)
      if(NOT EXISTS "${runtime_dll}")
        message(FATAL_ERROR
          "The bundled Assimp package is incomplete: ${runtime_dll} is missing. "
          "Required DLLs under third_party/msys2-ucrt64/ucrt64/bin must be "
          "included in the repository.")
      endif()
      add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${runtime_dll}"
                "$<TARGET_FILE_DIR:${EXECUTABLE_NAME}>")
    endforeach()

    # The bundled UCRT Assimp DLL and the MinGW-built executable depend on
    # these runtime libraries. Copy them from the active compiler toolchain so
    # the output directory can run without relying on the developer's PATH.
    get_filename_component(MINGW_RUNTIME_DIRECTORY
                           "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(MINGW_RUNTIME_DLL_NAMES
      libgcc_s_seh-1.dll
      libstdc++-6.dll
      libwinpthread-1.dll
      libbz2-1.dll
    )
    foreach(runtime_dll_name IN LISTS MINGW_RUNTIME_DLL_NAMES)
      unset(runtime_dll_path CACHE)
      find_file(runtime_dll_path
        NAMES "${runtime_dll_name}"
        HINTS "${MINGW_RUNTIME_DIRECTORY}"
        NO_DEFAULT_PATH)
      if(NOT runtime_dll_path)
        message(FATAL_ERROR
          "Required MinGW runtime ${runtime_dll_name} was not found beside "
          "${CMAKE_CXX_COMPILER}. Use a complete MSYS2 UCRT64 toolchain.")
      endif()
      add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${runtime_dll_path}"
                "$<TARGET_FILE_DIR:${EXECUTABLE_NAME}>")
      unset(runtime_dll_path CACHE)
      unset(runtime_dll_path)
    endforeach()
  endif()

elseif(UNIX)

  target_compile_options(${EXECUTABLE_NAME} PRIVATE -Wall -Wno-unused-function)

  # Add custom target for 'run'
  add_custom_target(run
      COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ./main
      DEPENDS main
      USES_TERMINAL
  )

  find_package(OpenGL REQUIRED)
  find_package(X11 REQUIRED)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  find_library(MATH_LIBRARY m)
  if(NOT MATH_LIBRARY)
    message(FATAL_ERROR "The system math library (libm) is required by miniaudio.")
  endif()
  target_link_libraries(${EXECUTABLE_NAME}
    ${CMAKE_DL_LIBS}
    ${MATH_LIBRARY}
    ${PROJECT_SOURCE_DIR}/lib-linux/libglfw3.a
    Threads::Threads
    ${OPENGL_LIBRARIES}
    ${X11_LIBRARIES}
    ${X11_Xrandr_LIB}
    ${X11_Xcursor_LIB}
    ${X11_Xinerama_LIB}
  )

endif()
