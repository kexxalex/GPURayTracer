#pragma once

#ifdef __linux__
#include <glm/glm.hpp>
#elif _WIN32
#include "glm/glm.hpp"
#endif

struct Material {
    Material &operator=(const Material &mat) = default;
    glm::fvec4 albedo;
    glm::fvec4 specular_roughness;

    glm::fvec4 emission_ior;
};
