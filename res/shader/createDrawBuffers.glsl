#version 450 core

layout (local_size_x=64, local_size_y=1, local_size_z=1) in;



struct Vec3 {
    float x, y, z;
};

struct Vec2 {
    float s, t;
};

struct TriangleModel {
    Vec3 true_normal;
    Vec3 position;
    Vec3 u;
    Vec3 v;
};

struct TriangleShading {
    uint material_id;

    Vec3 normals[3];
    Vec3 tangents[3];

    Vec2 tex_p;
    Vec2 tex_u;
    Vec2 tex_v;
};

struct Material {
    vec4 albedo;
    vec4 specular_roughness;
    vec4 emission_ior;
};

layout(std430, binding=1) restrict readonly buffer triangleModelBuffer {
    TriangleModel triangleModels[];
};

layout(std430, binding=2) restrict readonly buffer triangleShadingBuffer {
    TriangleShading triangleShadings[];
};

layout(std430, binding=3) restrict readonly buffer materialBuffer {
    Material materials[];
};

struct Vertex {
    vec4 position;
    vec4 normal;
    vec4 tangent;
    vec4 albedo;
    vec4 specular_roughness;
    vec4 emission_ior;
    vec4 uv_mat_id;
};


layout(std430, binding=5) restrict writeonly buffer vertexBuffer {
    Vertex vertices[];
};

void main(void) {
    const uint triID = uint(gl_GlobalInvocationID.x);

    const TriangleModel tri = triangleModels[triID];
    const TriangleShading shade = triangleShadings[triID];
    const Material material = materials[shade.material_id];

    vertices[3*triID].position = vec4(tri.position.x, tri.position.y, tri.position.z, 1.0);
    vertices[3*triID].normal = vec4(shade.normals[0].x, shade.normals[0].y, shade.normals[0].z, 0);
    vertices[3*triID].tangent = vec4(shade.tangents[0].x, shade.tangents[0].y, shade.tangents[0].z, 0);
    vertices[3*triID].albedo = vec4(material.albedo.rgb, 0.0);
    vertices[3*triID].specular_roughness = material.specular_roughness;
    vertices[3*triID].emission_ior = material.emission_ior;
    vertices[3*triID].uv_mat_id = vec4(shade.tex_p.s, shade.tex_p.t, 0.0, shade.material_id);

    vertices[3*triID+1].position = vec4(tri.position.x+tri.u.x, tri.position.y+tri.u.y, tri.position.z+tri.u.z, 1.0);
    vertices[3*triID+1].normal = vec4(shade.normals[1].x, shade.normals[1].y, shade.normals[1].z, 0);
    vertices[3*triID+1].tangent = vec4(shade.tangents[1].x, shade.tangents[1].y, shade.tangents[1].z, 0);
    vertices[3*triID+1].albedo = vec4(material.albedo.rgb, 0.0);
    vertices[3*triID+1].specular_roughness = material.specular_roughness;
    vertices[3*triID+1].emission_ior = material.emission_ior;
    vertices[3*triID+1].uv_mat_id = vec4(shade.tex_p.s+shade.tex_u.s, shade.tex_p.t+shade.tex_u.t, 0.0, shade.material_id);

    vertices[3*triID+2].position = vec4(tri.position.x+tri.v.x, tri.position.y+tri.v.y, tri.position.z+tri.v.z, 1.0);
    vertices[3*triID+2].normal = vec4(shade.normals[2].x, shade.normals[2].y, shade.normals[2].z, 0);
    vertices[3*triID+2].tangent = vec4(shade.tangents[2].x, shade.tangents[2].y, shade.tangents[2].z, 0);
    vertices[3*triID+2].albedo = vec4(material.albedo.rgb, 0.0);
    vertices[3*triID+2].specular_roughness = material.specular_roughness;
    vertices[3*triID+2].emission_ior = material.emission_ior;
    vertices[3*triID+2].uv_mat_id = vec4(shade.tex_p.s+shade.tex_v.s, shade.tex_p.t+shade.tex_v.t, 0.0, shade.material_id);
}
