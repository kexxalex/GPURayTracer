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
    Triangle() : position(0.0f), u(0.0f), v(0.0f), normals{}, tex_p(0.0f), tex_u(0.0f), tex_v(0.0f) {}
    
    constexpr Triangle(const glm::fvec3 &p0, const glm::fvec3 &p1, const glm::fvec3 &p2,
        const glm::fvec3 &n0, const glm::fvec3 &n1, const glm::fvec3 &n2,
        const glm::fvec2 &t0, const glm::fvec2 &t1, const glm::fvec2 &t2,
        unsigned int mat)
        : position(p0), u(p1 - p0), v(p2 - p0)
        , normals{n0,n1,n2}
        , true_normal(glm::cross(u, v))
        , tex_p(t0), tex_u(t1 - t0), tex_v(t2 - t0)
        , material_id(mat)
    {
        const glm::fvec3 (&N)[3] = normals;
        for (int i=0; i < 3; i++) {
            const glm::fvec3 T = tex_v.x * u - tex_u.x * v;
            tangents[i] = glm::normalize(T - glm::dot(N[i], T)*N[i]);
        }
    }

    Triangle &operator=(const Triangle &tri) {
        position = tri.position;
        u = tri.u;
        v = tri.v;

        tex_p = tri.tex_p;
        tex_u = tri.tex_u;
        tex_v = tri.tex_v;

        for (int i=0; i < 3; i++) {
            normals[i] = tri.normals[i];
            tangents[i] = tri.tangents[i];
        }
    
        true_normal = tri.true_normal;
        material_id = tri.material_id;

        return *this;
    }


    glm::fvec3 position;
    glm::fvec3 u;
    glm::fvec3 v;

    glm::fvec3 normals[3];
    glm::fvec3 tangents[3];
    glm::fvec3 true_normal;

    glm::fvec2 tex_p;
    glm::fvec2 tex_u;
    glm::fvec2 tex_v;

    unsigned int material_id;
};


struct Object {
    Object(std::string &&name) : name(std::move(name)) {}

    std::string name;
    std::vector<Triangle> triangles;
    int material_index;
};
