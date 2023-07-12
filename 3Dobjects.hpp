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
    constexpr Triangle() {};
    Triangle(const glm::fvec3 &p0, const glm::fvec3 &p1, const glm::fvec3 &p2,
             const glm::fvec3 &n0, const glm::fvec3 &n1, const glm::fvec3 &n2, unsigned int mat)
            : position(p0, 1.0f), u(p1 - p0, mat), v(p2 - p0, 0.0f),
              normal0(n0, 0.0f), normal1(n1, 0.0f), normal2(n2, 0.0f)
    {
        const auto true_normal(glm::cross(glm::fvec3(v), glm::fvec3(u)));
        normal0.w = true_normal.x;
        normal1.w = true_normal.y;
        normal2.w = true_normal.z;
    }

    Triangle &operator=(const Triangle &tri) {
        position = tri.position;
        u = tri.u;
        v = tri.v;

        normal0 = tri.normal0;
        normal1 = tri.normal1;
        normal2 = tri.normal2;

        return *this;
    };

    glm::fvec4 position;
    glm::fvec4 u;
    glm::fvec4 v;

    glm::fvec4 normal0;
    glm::fvec4 normal1;
    glm::fvec4 normal2;
};


struct Object {
    Object(std::string &&name) : name(name) {}

    std::string name;
    std::vector<Triangle> triangles;
    int material_index;
};
