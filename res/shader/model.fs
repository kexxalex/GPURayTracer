#version 450 core

layout (location=0) out vec3 outFragColor;


const vec3 LIGHT_DIR = normalize(vec3(0, -1, -1))*0.5;
const vec3 LUMA = vec3(0.299, 0.587, 0.114);

in vec3 vNormal;
in vec3 vAlbedo;
in vec3 vSpecular;
in vec3 vEmission;
in vec3 vVertex;

uniform mat4 MVP;
uniform vec3 CAMERA;

void main() {
    vec3 normal = normalize(vNormal);
    float light = max(dot(-LIGHT_DIR, normal), 0.0);
    vec3 view = normalize((vec4(vVertex-CAMERA,0)).xyz);
    float spec = max(dot(reflect(LIGHT_DIR, normal), -view), 0.0);
    float intensity = dot(LUMA, vAlbedo + vSpecular);
    outFragColor = light*vAlbedo + spec*vSpecular + vEmission + (vAlbedo + vSpecular*max(dot(view, -normal), 0.0))*vec3(0.8, 0.86, 0.9)*intensity;
} 