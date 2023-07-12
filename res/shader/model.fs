#version 450 core

layout (location=0) out vec3 outFragColor;

const vec3 LUMA = vec3(0.299, 0.587, 0.114);

in vec3 vNormal;
in vec3 vAlbedo;
in vec3 vSpecular;
in vec3 vEmission;
in vec3 vVertex;
in float vIOR;
in float vRoughness;

uniform vec3 LIGHT_DIR;
uniform vec3 AMBIENT;
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

void main() {
    vec3 normal = normalize(vNormal);
    vec3 view = normalize(vVertex - CAMERA);

    const float light = max(-dot(normal,LIGHT_DIR), 0.0);
    const vec3 reflected = reflect(view, normal);
    const float specIntens = max(-dot(reflected, LIGHT_DIR), 0.0);
    const float specAmbientIntens = max(-dot(view, normal), 0.0);

    const float transmissionAmb = (vIOR == 0.0) ? 0.0 : getTransmission(specAmbientIntens, 1.00029, vIOR);

    const float mCosThetaAmb = 1.0 - specAmbientIntens;
    const float mCosThetaAmb2 = mCosThetaAmb * mCosThetaAmb;
    const float mCosThetaAmb4 = mCosThetaAmb2 * mCosThetaAmb2;
    const float mCosThetaAmb5 = mCosThetaAmb * mCosThetaAmb4;

    const float fresnelAmb = (1.0-transmissionAmb) * mCosThetaAmb5;
    const float reflectanceAmb = transmissionAmb + fresnelAmb;

    const float transmission = (vIOR == 0.0) ? 0.0 : getTransmission(specIntens, 1.00029, vIOR);

    const float mCosTheta = 1.0 - specIntens;
    const float mCosTheta2 = mCosTheta * mCosTheta;
    const float mCosTheta4 = mCosTheta2 * mCosTheta2;
    const float mCosTheta5 = mCosTheta * mCosTheta4;

    const float fresnel = (1.0-transmission) * mCosTheta5;
    const float reflectance = transmission + fresnel;


    const float a_s_lerp = clamp((specIntens + specAmbientIntens + reflectance + reflectanceAmb)*0.5, 0.0, 1.0) * (1.0 - vRoughness);

    outFragColor = WIREFRAME ? normal * 0.5 + 0.5 : vEmission + mix(vAlbedo, vSpecular, a_s_lerp) * (AMBIENT + vec3(light + pow(specIntens, vRoughness) + (1.0 - max(-dot(view, normal), 0.0))));
} 
