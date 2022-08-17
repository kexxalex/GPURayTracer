#version 450 core

layout (location=0) out vec3 outFragColor;


const vec3 LUMA = vec3(0.299, 0.587, 0.114);

in vec3 vNormal;
in vec3 vAlbedo;
in vec3 vSpecular;
in vec3 vEmission;
in vec3 vVertex;

uniform vec3 LIGHT_DIR;
uniform vec3 AMBIENT;
uniform mat4 MVP;
uniform vec3 CAMERA;

void main() {
    vec3 normal = normalize(vNormal);
    float light = max(dot(-LIGHT_DIR, normal), 0.0);
    vec3 view = normalize((vec4(vVertex-CAMERA,0)).xyz);
    float spec = max(dot(reflect(LIGHT_DIR, normal), -view), 0.0);
    float ambient_light = dot(LUMA, vAlbedo + vSpecular);
    float ambient_spec = max(dot(view, -normal), 0.0);

    vec3 diffuse = light*vAlbedo;
    vec3 specular = spec*vSpecular;
    outFragColor = diffuse + specular + vEmission + (vAlbedo + vSpecular*ambient_spec)*AMBIENT*ambient_light;
} 