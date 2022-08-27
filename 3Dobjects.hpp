#pragma once

#include <string>
#include <vector>

#ifdef __linux__
#include <glm/glm.hpp>
#elif _WIN32
#include "glm/glm.hpp"
#endif


struct Vertex {
    glm::vec4 position;
    glm::vec4 normal;
    glm::vec4 albedo;
    glm::vec4 specular;
    glm::fvec4 emission;
};


struct Ray {
    Ray(const glm::fvec3 &o, const glm::fvec3 &d)
            : position(o, 1.0f), direction(d, 0.0f) {}

    Ray &operator=(const Ray &tri) = default;

    glm::fvec4 position;
    glm::fvec4 direction;
};


struct Triangle {
    Triangle(const glm::fvec3 &p0, const glm::fvec3 &p1, const glm::fvec3 &p2,
             const glm::fvec3 &n0, const glm::fvec3 &n1, const glm::fvec3 &n2, unsigned int mat)
            : position(p0, 1.0f), u(p1 - p0, mat), v(p2 - p0, 0.0f),
              normals0(n0, 0.0f), normals1(n1, 0.0f), normals2(n2, 0.0f)
    {}

    Triangle &operator=(const Triangle &tri) = default;

    glm::fvec4 position;
    glm::fvec4 u;
    glm::fvec4 v;

    glm::fvec4 normals0;
    glm::fvec4 normals1;
    glm::fvec4 normals2;
};


struct Object {
    Object(std::string &&name) : name(name) {}

    std::string name;
    std::vector<Triangle> triangles;
    int material_index;
};
