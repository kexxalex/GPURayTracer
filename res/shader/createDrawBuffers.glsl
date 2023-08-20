#version 450 core

layout (local_size_x=64, local_size_y=1, local_size_z=1) in;


struct Vec3 {
    float x, y, z;
};

struct Vec2 {
    float s, t;
};

struct Triangle {
    Vec3 position;
    Vec3 u;
    Vec3 v;

    Vec3 normals[3];
    Vec3 tangents[3];
    Vec3 true_normal;

    Vec2 tex_p;
    Vec2 tex_u;
    Vec2 tex_v;

    uint material_id;
};

struct Material {
    vec4 albedo;
    vec4 specular_roughness;
    vec4 emission_ior;
};

struct Vertex {
    vec4 position;
    vec4 normal;
    vec4 albedo;
    vec4 specular_roughness;
    vec4 emission_ior;
};


// Materials
layout(std430, binding=1) restrict readonly buffer materialBuffer {
    Material materials[];
};

layout(std430, binding=2) restrict readonly buffer triangleBuffer {
    Triangle triangles[];
};


layout(std430, binding=5) restrict writeonly buffer vertexBuffer {
    Vertex vertices[];
};

void main(void) {
    const uint triID = uint(gl_GlobalInvocationID.x);

    const Triangle tri = triangles[triID];
    Material material = materials[tri.material_id];

    vertices[3*triID].position = vec4(tri.position.x, tri.position.y, tri.position.z, 1.0);
    vertices[3*triID].normal = vec4(tri.normals[0].x, tri.normals[0].y, tri.normals[0].z, 0);
    vertices[3*triID].albedo = vec4(material.albedo.rgb, 0);
    vertices[3*triID].specular_roughness = material.specular_roughness;
    vertices[3*triID].emission_ior = material.emission_ior;

    vertices[3*triID+1].position = vec4(tri.position.x+tri.u.x, tri.position.y+tri.u.y, tri.position.z+tri.u.z, 1.0);
    vertices[3*triID+1].normal = vec4(tri.normals[1].x, tri.normals[1].y, tri.normals[1].z, 0);
    vertices[3*triID+1].albedo = vec4(material.albedo.rgb, 0.0);
    vertices[3*triID+1].specular_roughness = material.specular_roughness;
    vertices[3*triID+1].emission_ior = material.emission_ior;

    vertices[3*triID+2].position = vec4(tri.position.x+tri.v.x, tri.position.y+tri.v.y, tri.position.z+tri.v.z, 1.0);
    vertices[3*triID+2].normal = vec4(tri.normals[2].x, tri.normals[2].y, tri.normals[2].z, 0);
    vertices[3*triID+2].albedo = vec4(material.albedo.rgb, 0.0);
    vertices[3*triID+2].specular_roughness = material.specular_roughness;
    vertices[3*triID+2].emission_ior = material.emission_ior;
}
