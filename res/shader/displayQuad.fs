#version 450 core

layout (location=0) out vec3 outFragColor;
layout (binding=0) uniform sampler2D tex;

uniform int SAMPLES;

in vec2 vUV;

void main() {
    outFragColor = texture(tex, vUV).rgb;// / SAMPLES;
} 