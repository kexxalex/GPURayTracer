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

struct Ray {
	vec4 position;
	vec4 direction;
};



layout(std430, binding=1) restrict writeonly buffer visBuffer {
	int visibility[];
};

layout(std430, binding=3) restrict readonly buffer triangleBuffer {
	Triangle triangles[];
};


uniform int COUNT;


bool isVisible(int caster, int receiver) {
	Triangle c = triangles[caster];
	Triangle r = triangles[receiver];

	// cast rays from every vertex of triangle caster with direction of its normal
	// and check whether it can hit the triangle receiver by calculating the dot product with all 9 difference vectors

	return (
		dot(c.normal0.xyz, c.position.xyz - r.position.xyz) < 0 ||
		dot(c.normal1.xyz, c.position.xyz + c.u.xyz - r.position.xyz) < 0 ||
		dot(c.normal2.xyz, c.position.xyz + c.v.xyz - r.position.xyz) < 0 ||

		dot(c.normal0.xyz, c.position.xyz - r.position.xyz - r.u.xyz) < 0 ||
		dot(c.normal1.xyz, c.position.xyz + c.u.xyz - r.position.xyz - r.u.xyz) < 0 ||
		dot(c.normal2.xyz, c.position.xyz + c.v.xyz - r.position.xyz - r.u.xyz) < 0 ||

		dot(c.normal0.xyz, c.position.xyz - r.position.xyz - r.v.xyz) < 0 ||
		dot(c.normal1.xyz, c.position.xyz + c.u.xyz - r.position.xyz - r.v.xyz) < 0 ||
		dot(c.normal2.xyz, c.position.xyz + c.v.xyz - r.position.xyz - r.v.xyz) < 0
	);
}

void main(void) {
	ivec2 texel = ivec2(gl_GlobalInvocationID.x, 0);
	for (int recvID=0; recvID < COUNT; ++recvID) {
		if (texel.x != recvID && isVisible(texel.x, recvID)) {
			visibility[texel.x * COUNT + texel.y] = recvID;
			++texel.y;
		}
	}
	visibility[texel.x * COUNT + texel.y] = texel.x;
}
