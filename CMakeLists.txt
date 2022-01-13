cmake_minimum_required(VERSION 3.16.0)
project(GPUFractals LANGUAGES C CXX)

if (TEX_SIZE)
    set(CMAKE_TEX_SIZE ${TEX_SIZE})
else()
    set(CMAKE_TEX_SIZE 4096)
endif()
set(SHADERS_DIR "\"${CMAKE_BINARY_DIR}/shaders\"")


# Include GLAD. Allow for other implementations
if (NOT DEFINED GLAD_HOME)
    set(GLAD_HOME "ext/GLAD")
endif()
include_directories("${GLAD_HOME}/include")
add_library(GLAD STATIC "${GLAD_HOME}/src/glad.c")

# Include GLFW3. Allow for other implementations
if (NOT DEFINED GLFW3_HOME)
    set(GLFW3_HOME "ext/GLFW")
endif()
set(glfw3_DIR "${GLFW3_HOME}/lib/cmake/glfw3")
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

# Include STB. Allow for other implementations
if (NOT DEFINED STB_HOME)
    set(STB_HOME "ext/STB")
endif()
include_directories("${STB_HOME}/include")
add_library(STB STATIC "${STB_HOME}/stb_implementation.c")

# Create the executable
include_directories("${CMAKE_SOURCE_DIR}/include")
configure_file("${CMAKE_SOURCE_DIR}/include/defines.hpp.in" "${CMAKE_SOURCE_DIR}/include/defines.hpp")
add_executable(GPUFractals src/gpu_fractals.cpp)
target_link_libraries(GPUFractals GLAD STB glfw OpenGL::GL)

# Copy the shaders
file(COPY "${CMAKE_SOURCE_DIR}/shaders" DESTINATION "${CMAKE_BINARY_DIR}")