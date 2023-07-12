#version 450 core

layout (local_size_x=64, local_size_y=1, local_size_z=1) in;


struct Triangle {
	vec4 position;
	vec4 u;
	vec4 v;

	vec4 normal0;
	vec4 normal1;
	vec4 normal2;
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


layout(std430, binding=3) restrict readonly buffer triangleBuffer {
	Triangle triangles[];
};

// Materials
layout(std430, binding=6) restrict readonly buffer materialBuffer {
	Material materials[];
};


layout(std430, binding=5) restrict writeonly buffer vertexBuffer {
	Vertex vertices[];
};

void main(void) {
	const uint triID = uint(gl_GlobalInvocationID.x);

	const Triangle tri = triangles[triID];
	Material material = materials[uint(ceil(tri.u.w))];

	vertices[3*triID].position = vec4(tri.position.xyz, 1.0);
	vertices[3*triID].normal = vec4(tri.normal0.xyz, 0);
	vertices[3*triID].albedo = vec4(material.albedo.rgb, 0);
	vertices[3*triID].specular_roughness = material.specular_roughness;
	vertices[3*triID].emission_ior = material.emission_ior;

	vertices[3*triID+1].position = vec4(tri.position.xyz + tri.u.xyz, 1.0);
	vertices[3*triID+1].normal = vec4(tri.normal1.xyz, 0.0);
	vertices[3*triID+1].albedo = vec4(material.albedo.rgb, 0.0);
	vertices[3*triID+1].specular_roughness = material.specular_roughness;
	vertices[3*triID+1].emission_ior = material.emission_ior;

	vertices[3*triID+2].position = vec4(tri.position.xyz + tri.v.xyz, 1.0);
	vertices[3*triID+2].normal = vec4(tri.normal2.xyz, 0.0);
	vertices[3*triID+2].albedo = vec4(material.albedo.rgb, 0.0);
	vertices[3*triID+2].specular_roughness = material.specular_roughness;
	vertices[3*triID+2].emission_ior = material.emission_ior;
}
