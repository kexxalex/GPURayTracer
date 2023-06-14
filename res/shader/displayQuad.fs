#version 450 core

layout (location=0) out vec3 outFragColor;
layout (binding=0) uniform sampler2D tex;

uniform int SAMPLES;

in vec2 vUV;

vec3 ACESFilm(in vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    outFragColor = ACESFilm(texture(tex, vUV).rgb);// / SAMPLES;
} 