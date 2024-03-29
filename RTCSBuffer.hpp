#pragma once


#ifdef __linux__
#include <GL/glew.h>
#include <glm/glm.hpp>
#elif _WIN32
#include "GL/glew.h"
#include "glm/glm.hpp"
#endif

// RayTracer ComputeShader Data
struct st_RTCS_data {
    ~st_RTCS_data() {
        glDeleteTextures(2, &renderTarget);
        glDeleteBuffers(3, buffer.arr);
    }
    bool initialized{false};
    glm::ivec2 resolution{0, 0};
    glm::ivec2 buffer_res{0, 0};

    uint32_t triangles{ 0 };

    GLuint renderTarget{0};
    GLuint renderTargetLow{0};

    union {
        GLuint arr[3];
        struct {
            GLuint models;
            GLuint shading;
            GLuint materials;
        };
    } buffer;
};
