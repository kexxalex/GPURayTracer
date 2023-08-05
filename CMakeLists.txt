cmake_minimum_required(VERSION 3.5)
project(gpuRT)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})


add_executable(gpuRT RayTracer.cpp Scene.cpp Shader.cpp)
target_link_libraries(gpuRT -lGL -lGLEW -lglfw -mfma)
