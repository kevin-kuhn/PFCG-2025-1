cmake_minimum_required(VERSION 3.10)
project(CGCCHIB)

# Define o padrão do C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Ativa o FetchContent
include(FetchContent)

# Configura o FetchContent para baixar e compilar GLFW automaticamente
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4  # Define a versão desejada da GLFW
)

# Faz o download e compila a GLFW
FetchContent_MakeAvailable(glfw)

# Adiciona as pastas de cabeçalhos
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/glad)
include_directories(${CMAKE_SOURCE_DIR}/common)

# Lista de exercícios (por enquanto só tem o HelloTriangle)
set(EXERCISES
    Hello3D
)

find_package(OpenGL REQUIRED)

add_compile_options(-Wno-pragmas)

add_executable(EXERC
    Mod1/Hello3D.cpp
    Common/glad.c
)

# Inclui cabeçalhos
target_include_directories(EXERC PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include/glad"
    "${CMAKE_SOURCE_DIR}/common"
)

# Linka com a GLFW e o OpenGL encontrado
target_link_libraries(EXERC PRIVATE
    glfw
    OpenGL::GL
)

# Para macOS, se precisar manualmente de frameworks do sistema:
if (APPLE)
    target_link_libraries(EXERC PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
endif()