#pragma once

#include <glm/glm.hpp>

struct Material {
    Material &operator=(const Material &mat) = default;
    glm::fvec4 albedo;
    glm::fvec4 specular;

    glm::fvec4 emission_metallic;
};
