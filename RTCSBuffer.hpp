#pragma once

#include <GL/glew.h>

// RayTracer ComputeShader Data
struct st_RTCS_data {
    ~st_RTCS_data() {
        glDeleteTextures(2, &renderTarget);
        glDeleteBuffers(5, &visibility);
    }
    bool initialized{false};
    glm::ivec2 resolution{0, 0};
    glm::ivec2 buffer_res{0, 0};

    unsigned int triangles{ 0 };

    GLuint renderTarget{0};
    GLuint renderTargetLow{0};

    GLuint visibility{0};

    GLuint triangleBuffer{0};

    GLuint albedo{0};
    GLuint specular{0};
    GLuint emission_metallic{0};
};
