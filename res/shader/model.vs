#version 450 core

layout(location=0) in vec4 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aAlbedo;
layout(location=3) in vec4 aSpecularRoughness;
layout(location=4) in vec4 aEmissionIOR;

uniform mat4 MVP;

out vec3 vNormal;
out vec3 vAlbedo;
out vec3 vSpecular;
out vec3 vEmission;
out vec3 vVertex;
out float vIOR;
out float vRoughness;

void main() {
	vNormal = aNormal;
	vAlbedo = aAlbedo;
	vSpecular = aSpecularRoughness.rgb;
	vRoughness = aSpecularRoughness.a;
	vEmission = aEmissionIOR.rgb;
	vVertex = aPosition.xyz;
	vIOR = aEmissionIOR.a;
	gl_Position = MVP * (vec4(1,1,-1,1) * aPosition.xyzw);
}