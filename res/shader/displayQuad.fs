#version 450 core

layout (location=0) out vec3 outFragColor;
layout (binding=0) uniform sampler2D tex;

uniform uint SAMPLES;

in vec2 vUV;

vec3 ACESFilm(in vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), vec3(0.0), vec3(1.0));
}

vec3 LinearTosRGB(in vec3 C) {
    vec3 sRGB = vec3(0.0);
    sRGB.r = (C.r <= 0.0031308) ? C.r * 12.92 : 1.055 * pow(C.r, 1.0/2.4) - 0.055;
    sRGB.g = (C.g <= 0.0031308) ? C.g * 12.92 : 1.055 * pow(C.g, 1.0/2.4) - 0.055;
    sRGB.b = (C.b <= 0.0031308) ? C.b * 12.92 : 1.055 * pow(C.b, 1.0/2.4) - 0.055;
    return sRGB;
}

vec3 LinearToP3(in vec3 C) {
    return pow(C, vec3(1.0/2.6));
}

void main() {
    vec3 rgb = texture(tex, vUV).rgb;
    outFragColor = LinearTosRGB(rgb);
} 