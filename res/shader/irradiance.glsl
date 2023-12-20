#version 450 core

layout (local_size_x=64, local_size_y=1, local_size_z=1) in;

uniform layout(rgba32f, binding=0) restrict readonly image2D RADIANCE;
uniform layout(rgba32f, binding=1) restrict image2D IRRADIANCE;
uniform layout(location=1) int ITERATION;

const float PI = 3.141592653589793;
const float TWO_PI = 2.0 * PI;

vec3 equirectToNormal(in const vec2 uv) {
    return vec3(-sin(PI*uv.y)*cos(2.0*PI*uv.x), cos(PI*uv.y), -sin(PI*uv.y)*sin(2.0*PI*uv.x));
}

float uvDotUV(in const vec2 uv, in const vec2 UV) {
    return (cos(PI*uv.y)*cos(PI*UV.y) + cos(2.0*PI * (uv.x-UV.x)) * sin(PI*uv.y)*sin(PI*UV.y));
}

void main()
{
    const ivec2 SIZE = imageSize(RADIANCE);
    const ivec2 TEXEL = ivec2(gl_GlobalInvocationID.xy);
    const vec2 ISIZE = vec2(1.0) / vec2(SIZE);

    const vec2 UV = TEXEL * ISIZE * 4.0;
    const float renormation = 2.0 / SIZE.x / SIZE.y;

    // Fourier transformation would be faster
    vec3 irradiance = imageLoad(IRRADIANCE, TEXEL).rgb;

    for (int i=0; i < SIZE.x; i++)
    {
        const vec2 uv = vec2(i, ITERATION) * ISIZE;
        const float weight = max(uvDotUV(uv, UV), 0.0) * renormation;
        irradiance += weight * imageLoad(RADIANCE, ivec2(i, ITERATION)).rgb;
    }
    imageStore(IRRADIANCE, TEXEL, vec4(irradiance, 1.0));
}