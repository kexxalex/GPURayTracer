#version 450 core

layout (local_size_x=8, local_size_y=8, local_size_z=1) in;




struct Triangle {
	vec4 position;
	vec4 u;
	vec4 v;

	vec4 N;

	vec4 normal0;
	vec4 normal1;
	vec4 normal2;
};

struct Ray {
	vec4 position;
	vec4 direction;
};

struct Material {
	vec4 albedo;
	vec4 specular_roughness;
	vec4 emission_ior;
};

uniform mat4 CAMERA;


layout(rgba32f, binding=0) restrict uniform image2D img_output;



layout(std430, binding=1) restrict readonly buffer visBuffer { int visibility[]; };


layout(std430, binding=2) restrict readonly buffer rayBuffer {
	Ray rays[];
};


layout(std430, binding=3) restrict readonly buffer triangleBuffer {
	Triangle triangles[];
};

// Large array of random unit vectors

layout(std430, binding=4) restrict readonly buffer randBuffer {
	vec4 rand_vec[];
};

// Materials
layout(std430, binding=6) restrict readonly buffer materialBuffer {
	Material materials[];
};

uniform int COUNT;
uniform int RECURSION;
uniform int SAMPLE;
uniform vec3 LIGHT_DIR;
uniform vec3 AMBIENT;

const vec3 LUMA = vec3(0.299, 0.587, 0.114);
const float PI = 3.141592653589793;
const float INV_PI = 1.0/3.141592653589793;
uvec2 SIZE;
ivec2 TEXEL;

float LIGHT_INTENS;

vec3 random_hemi(vec3 n, uint step) {
	uint size = SIZE.x * SIZE.y * 4;
	vec3 rvec = rand_vec[((TEXEL.x * SIZE.y + TEXEL.y) * 13 + step + SAMPLE * 1123) % size].xyz;
	return dot(n, rvec) > 0 ? rvec : -rvec;
}

vec3 random_sphere(uint step) {
	uint size = SIZE.x * SIZE.y * 4;
	return rand_vec[((TEXEL.x * SIZE.y + TEXEL.y) * 17 + step + SAMPLE * 1123) % size].xyz;
}

bool intersect(in vec3 rayPos, in vec3 rayDir, in vec3 triPos, in vec3 u, in vec3 v, in vec3 n, inout vec3 isec) {
	float determinant = dot(rayDir, n);
	// Backface culling
	if (determinant <= 0.0)
		return false;

	vec3 delta = triPos - rayPos;
	float relative_depth = dot(n, delta);
	if (relative_depth <= 0)
		return false;
	
	// discard if the previous intersection is closer
	if (isec.z >= 0.0 && relative_depth > isec.z * determinant)
		return false;

	// some linear algebra to solve inverse(u, v, rayDir) * delta
	vec3 subDet = cross(rayDir, delta);
	vec3 relative = vec3(dot(subDet, v), -dot(subDet, u), relative_depth);

	if (relative.x <= 0.0 || relative.y <= 0.0 || relative.x + relative.y >= determinant)
		return false;

	float IDet = 1.0 / determinant;
	isec = relative * IDet;
	return true;
}

bool intersectShadow(in vec3 rayPos, in vec3 rayDir, in vec3 triPos, in vec3 u, in vec3 v, in vec3 n) {
	float determinant = -dot(rayDir, n);
	// Frontface culling
	if (determinant <= 0.0)
		return false;

	vec3 delta = rayPos - triPos;
	float relative_depth = dot(n, delta);
	if (relative_depth <= 0)
		return false;

	// some linear algebra to solve inverse(u, v, rayDir) * delta
	vec3 subDet = cross(rayDir, delta);
	vec2 relative = vec2(dot(subDet, v), -dot(subDet, u));

	return (relative.x >= 0.0 && relative.y >= 0.0 && relative.x + relative.y <= determinant);
}


bool hit(vec3 rayPos, vec3 rayDir, int avoid) {
	for (int triID = 0; triID < COUNT; ++triID) {
		if (triID == avoid)
			continue;

		Triangle tri = triangles[triID];
		if (intersectShadow(rayPos, rayDir, tri.position.xyz, tri.u.xyz, tri.v.xyz, tri.N.xyz))
			return true;
	}
	return false;
}

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


vec3 trace(vec3 rayPos, vec3 rayDir) {
	vec3 energy = vec3(0.0);
	vec3 path = vec3(1.0);
	
	int avoid_tri = -1;

	bool escaped = false;

	uint step = 0;
	for (; step < max(RECURSION, 1); ++step) {
		vec3 current_intersection = vec3(0.0, 0.0, -1.0);
		int current_tri = -1;
		for (int triID = 0; triID < COUNT; ++triID) {
			// int triID = (step > 0) ? visibility[avoid_tri * COUNT + visID] : visID;
			if (triID == avoid_tri)
				continue;

			Triangle tri = triangles[triID];
			
			if (intersect(rayPos, rayDir, tri.position.xyz, tri.u.xyz, tri.v.xyz, tri.N.xyz, current_intersection))
				current_tri = triID;
		}

		if (current_tri < 0) {
			energy += path * AMBIENT;
			break;
		}
	
		avoid_tri = current_tri;
		Triangle tri = triangles[current_tri];
		Material material = materials[uint(ceil(tri.u.w))];

		vec3 sec = current_intersection;
		vec3 normal = normalize(tri.normal0.xyz * (1.0-sec.x-sec.y) + tri.normal1.xyz * sec.x + tri.normal2.xyz * sec.y);

		vec3 refl_ray_dir = reflect(rayDir, normal);

		vec3 albedo = material.albedo.rgb;
		vec3 specular = material.specular_roughness.rgb;
		float scattering = material.specular_roughness.a*0.001;
		vec3 emission = material.emission_ior.rgb;
		float ior = material.emission_ior.a;

		rayPos += rayDir * sec.z;
		bool sun = LIGHT_INTENS > 0;
		float directLight = 0.0;
		float specIntens = max(dot(refl_ray_dir, normal), 0.0);

		if (sun) {
			vec3 light_scatter = normalize(LIGHT_DIR + 0.0049*random_hemi(LIGHT_DIR, step*7+3));
			float normal_dot_light = max(-dot(normal, light_scatter), 0.0);
			if (normal_dot_light > 0 && !hit(rayPos, -light_scatter, current_tri)) {
				sun = false;
				directLight = normal_dot_light*LIGHT_INTENS;
				specIntens += max(-dot(refl_ray_dir, light_scatter), 0.0)*LIGHT_INTENS*(1.0-scattering);
			}
		}

		if (sun || emission.r > 0 || emission.g > 0 || emission.b > 0) {
			energy += path * (emission + directLight);
			path = vec3(1.0);
		}

		float cosTheta = max(-dot(normal, rayDir), 0.0);
		
		float transmission = (ior == 0.0) ? 0.0 : getTransmission(cosTheta, 1.00029, ior);
		float fresnel = (1.0-transmission) * pow(1.0 - cosTheta, 5.0);
		float reflectance = transmission + fresnel;

		float lerp = clamp(specIntens+reflectance, 0.0, 1.0);
		vec3 scatter = random_hemi(normal, step);

		rayDir = normalize(refl_ray_dir + scatter*scattering);
		path *= mix(albedo, specular, lerp);
	}
	
	return energy;
}

void main(void) {
	LIGHT_INTENS = length(LIGHT_DIR);
	SIZE = imageSize(img_output);
	float inv_width = 1.0 / SIZE.x;
	float inv_height = 1.0 / SIZE.y;
	TEXEL = ivec2(gl_GlobalInvocationID.xy);

	vec4 color = vec4(0.0, 0.0, 0.0, 1.0);
	Ray ray;
	ray.position = vec4(0, 0, 0, 1.0);
	vec3 dtctor = vec3((TEXEL.x - 0.5*SIZE.x + 0.5)*inv_height, (TEXEL.y + 0.5)*1.0f*inv_height - 0.5, 0.5);
	ray.direction = vec4(normalize(dtctor - ray.position.xyz), 0.0);
	color.rgb = trace((CAMERA * ray.position).xyz, normalize(CAMERA * ray.direction).xyz);

	if (SAMPLE > 0) {
		vec3 prev = imageLoad(img_output, TEXEL).rgb;
		float inv_samples = 1.0 / (SAMPLE + 1.0);
		float scale = SAMPLE * inv_samples;
		imageStore(img_output, TEXEL, vec4(prev * scale + inv_samples * color.rgb, 1.0));
	}
	else
		imageStore(img_output, TEXEL, vec4(color.rgb, 1.0));
}
