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

void main() {
    vec3 normal = normalize(vNormal);
    float light = max(dot(-LIGHT_DIR, normal), 0.0);
    vec3 view = normalize((vec4(vVertex-CAMERA,0)).xyz);
    float spec = max(-dot(reflect(LIGHT_DIR, normal), view), 0.0);


    float transmission = (vIOR == 0.0) ? 1.0 : 1.0 - getTransmission(spec, 1.00029, vIOR);
    float fresnel = transmission * pow(1.0 - light, 5.0);
    float reflectance = (1.0 - transmission) + fresnel;

    vec3 surface = mix(vAlbedo, vSpecular, reflectance);
    float luma_total = dot(LUMA, surface);

    float ambient_light = mix(luma_total, 1.0, vRoughness);
    float ambient_spec = max(-dot(view, normal), 0.0);

    vec3 color = surface * light;// + AMBIENT * INV_PI * ambient_spec + vEmission;
    outFragColor = WIREFRAME ? normal * 0.5 + 0.5 : color;
} 
