cmake_minimum_required(VERSION 3.0)
project(gpuRT)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})


add_executable(gpuRT RayTracer.cpp Scene.cpp Shader.cpp)
target_link_libraries(gpuRT -lGL -lGLEW -lglfw)
