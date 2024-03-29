#version 450 core

layout (location=0) out vec3 outFragColor;
uniform layout(location=1) sampler2DArray ATLAS;
uniform layout(location=2) sampler2D RADIANCE;
uniform layout(location=3) sampler2D IRRADIANCE;
uniform layout(location=4) float EXPOSURE;

layout(std430, binding=6) restrict readonly buffer hasTextureBuffer {
    int hasTexture[];
};

const vec3 LUMA = vec3(0.299, 0.587, 0.114);

in vec3 vNormal;
in vec3 vTangent;
in vec3 vAlbedo;
in vec3 vSpecular;
in vec3 vEmission;
in vec3 vVertex;
in vec2 vUV;
in float vIOR;
in float vRoughness;
in flat int vMatID;

uniform mat4 MVP;
uniform vec3 CAMERA;

uniform bool WIREFRAME;

const float PI = 3.141592653589793;
const float INV_PI = 1.0/3.141592653589793;


float getTransmission(float cosThetaI, float etaI, float etaT) {
	float sinThetaI = sqrt(1.0 - cosThetaI*cosThetaI);
	float sinThetaT = etaI / etaT * sinThetaI;
	if (sinThetaT >= 1.0)
		return 1.0;
	
	float cosThetaT = sqrt(1.0 - sinThetaT*sinThetaT);
	vec2 reflectance = vec2(
		(etaT * cosThetaI - etaI * cosThetaT) / (etaT * cosThetaI + etaI * cosThetaT),
		(etaI * cosThetaI - etaT * cosThetaT) / (etaI * cosThetaI + etaT * cosThetaT)
	);

	return dot(reflectance, reflectance) * 0.5;
}

vec3 ACESFilm(in vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

vec3 LinearTosRGB(in vec3 C) {
    vec3 sRGB = vec3(0.0);
    sRGB.r = (C.r <= 0.0031308) ? C.r * 12.92 : 1.055 * pow(C.r, 1.0/2.4) - 0.055;
    sRGB.g = (C.g <= 0.0031308) ? C.g * 12.92 : 1.055 * pow(C.g, 1.0/2.4) - 0.055;
    sRGB.b = (C.b <= 0.0031308) ? C.b * 12.92 : 1.055 * pow(C.b, 1.0/2.4) - 0.055;
    return sRGB;
}

vec3 sRGBtoLinear(in vec3 C) { return pow((C + 0.055)/1.055, vec3(2.4)); }

vec3 skyColor(in const vec3 direction) {
    const vec2 uv = vec2(atan(direction.z, direction.x) * INV_PI * 0.5, -asin(direction.y) * INV_PI) + vec2(0.5);
    return texture(RADIANCE, uv).rgb * EXPOSURE;
}

vec3 skyColorDiffuse(in const vec3 direction) {
    const vec2 uv = vec2(atan(direction.z, direction.x) * INV_PI * 0.5, -asin(direction.y) * INV_PI) + vec2(0.5);
    return texture(IRRADIANCE, uv).rgb * EXPOSURE;
}

void main() {
    const bool hasTex = ( hasTexture[vMatID] == 1 );
    const vec3 N = normalize(vNormal);

    const vec3 T = normalize(vTangent - N * dot(N, vTangent));
    const mat3 TBNi = mat3(T, cross(T, N), N);
    const vec3 tex_normal = normalize(texture(ATLAS, vec3(vUV, vMatID*3 + 1)).rgb*2.0-1.0);
    const vec3 normal = hasTex ? normalize(TBNi * tex_normal) : N;

    const vec3 view = normalize(vVertex - CAMERA);
    const vec3 reflected = reflect(view, normal);

    const float NdV = max(-dot(view, normal), 0.0);
    const float NdR = max(dot(normal, reflected), 0.0);

    const vec3 n_irradiance = skyColorDiffuse(normal);
    const vec3 n_radiance = skyColor(normal);
    const vec3 r_irradiance = skyColorDiffuse(reflected);
    const vec3 r_radiance = skyColor(reflected);

    vec3 diffuse;
    vec3 specular = vec3(1.0);

    float roughness = vRoughness;
    float metallic = float(vRoughness < 0.125);

    if (hasTex) {
        vec3 tex_color = texture(ATLAS, vec3(vUV, vMatID*3 + 0)).rgb;
        vec3 arm = texture(ATLAS, vec3(vUV, vMatID*3 + 2)).rgb;
        diffuse = tex_color;
        specular *= tex_color * arm.x;
        roughness = arm.y;
        metallic = arm.z;
    }
    else {
        diffuse = vAlbedo;
        specular *= vSpecular;
    }

    const float transmission = (vIOR == 0.0) ? 0.0 : getTransmission(NdV, 1.00029, vIOR);
    const float fresnel_reflectance = fma(1.0-transmission, pow(1.0-NdV, 5), transmission);

    specular *= r_radiance * (1.0 - roughness + fresnel_reflectance) * metallic + r_irradiance * fresnel_reflectance * (1.0-roughness);
    diffuse *= n_irradiance * INV_PI * (1.0 - metallic);

    vec3 color = diffuse + specular + vEmission;

    outFragColor = WIREFRAME ? normal * 0.5 + 0.5 : clamp(LinearTosRGB(color), vec3(0.0), vec3(1.0));
} 
