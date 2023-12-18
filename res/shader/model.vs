#version 450 core

layout(location=0) in vec4 aPosition;
layout(location=1) in vec4 aNormal;
layout(location=2) in vec4 aTangent;
layout(location=3) in vec4 aAlbedo;
layout(location=4) in vec4 aSpecularRoughness;
layout(location=5) in vec4 aEmissionIOR;
layout(location=6) in vec4 aUV_MatID;

uniform mat4 MVP;

out vec3 vNormal;
out vec3 vTangent;
out vec3 vAlbedo;
out vec3 vSpecular;
out vec3 vEmission;
out vec3 vVertex;
out vec2 vUV;
out float vIOR;
out float vRoughness;
out int vMatID;

void main() {
	vNormal = aNormal.xyz;
	vTangent = aTangent.xyz;
	vAlbedo = aAlbedo.xyz;
	vSpecular = aSpecularRoughness.rgb;
	vRoughness = aSpecularRoughness.a;
	vEmission = aEmissionIOR.rgb;
	vVertex = aPosition.xyz;
	vIOR = aEmissionIOR.a;
	vUV = aUV_MatID.xy;
	vMatID = int(aUV_MatID.w);
	gl_Position = MVP * (vec4(1,1,-1,1) * aPosition.xyzw);
}