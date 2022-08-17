#pragma once

#include <glm/glm.hpp>

struct Material {
    glm::fvec3 albedo;
    glm::fvec3 specular;

    glm::fvec3 emission;
    float metallic{0.0f};
};
