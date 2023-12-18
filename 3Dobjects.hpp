#pragma once

#include <string>
#include <vector>

#ifdef __linux__
#include <glm/glm.hpp>
#elif _WIN32
#include "glm/glm.hpp"
#endif



struct Vertex {
    glm::fvec4 position;
    glm::fvec4 normal;
    glm::fvec4 tangent;
    glm::fvec4 albedo;
    glm::fvec4 specular_roughness;
    glm::fvec4 emission_ior;
    glm::fvec4 uv_mat_id;
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
        , tangents{{}, {}, {}}
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

struct TriangleModel {
    glm::fvec3 true_normal;
    glm::fvec3 position;
    glm::fvec3 span_u;
    glm::fvec3 span_v;

    TriangleModel() : true_normal(0.0f), position(0.0f), span_u(0.0f), span_v(0.0f) {}
    TriangleModel(const glm::fvec3& N, const glm::fvec3 &P, const glm::fvec3 &U, const glm::fvec3 &V)
        : true_normal(N), position(P), span_u(U), span_v(V)
    {}
    TriangleModel &operator=(const TriangleModel &tri) = delete;
};

struct TriangleShading {
    uint32_t material_id;

    glm::fvec3 normals[3];
    glm::fvec3 tangents[3];

    glm::fvec2 tex_p;
    glm::fvec2 tex_u;
    glm::fvec2 tex_v;

    TriangleShading()
        : material_id(0)
        , normals{glm::fvec3(0.0f), glm::fvec3(0.0f), glm::fvec3(0.0f)}
        , tangents{glm::fvec3(0.0f), glm::fvec3(0.0f), glm::fvec3(0.0f)}
        , tex_p(0.0f), tex_u(0.0f), tex_v(0.0f)
    {}
    TriangleShading(uint32_t mat, const glm::fvec3 (&n)[3], const glm::fvec3 (&t)[3], const glm::fvec2 &p, const glm::fvec2 &u, const glm::fvec2 &v)
        : material_id(mat), normals{n[0], n[1], n[2]}, tangents{t[0], t[1], t[2]}, tex_p(p), tex_u(u), tex_v(v)
    {}
    TriangleShading &operator=(const TriangleShading &tri) = delete;
};