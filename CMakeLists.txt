cmake_minimum_required(VERSION 3.16.0)
project(GPUFractals LANGUAGES C CXX)


option(ENABLE_REPRODUCIBILITY "Decide to always have the same random values or change them at every run." ON)
if (TEX_SIZE)
    set(CMAKE_TEX_SIZE ${TEX_SIZE})
else()
    set(CMAKE_TEX_SIZE 4096)
endif()
set(SHADERS_DIR "\"${CMAKE_BINARY_DIR}/shaders\"")


# Include GLFW
list(APPEND CMAKE_PREFIX_PATH "ext/GLFW")
find_package(glfw3 3.3 REQUIRED)
include_directories("ext/GLFW/include")
find_package(OpenGL REQUIRED)

# Include GLAD
include_directories("ext/GLAD/include")
add_library(GLAD STATIC "ext/GLAD/src/glad.c")

# Include STB
include_directories("ext/STB")
add_library(STB STATIC "ext/STB/stb_implementation.c")

# Create the executable
include_directories("${CMAKE_SOURCE_DIR}/include")
configure_file("${CMAKE_SOURCE_DIR}/include/defines.hpp.in" "${CMAKE_SOURCE_DIR}/include/defines.hpp")
add_executable(GPUFractals src/gpu_fractals.cpp)
target_link_libraries(GPUFractals GLAD STB glfw OpenGL::GL)

# Copy the shaders
file(COPY "${CMAKE_SOURCE_DIR}/shaders" DESTINATION "${CMAKE_BINARY_DIR}")