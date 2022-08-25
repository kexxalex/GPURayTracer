#version 450 core

layout (local_size_x=8, local_size_y=8, local_size_z=1) in;




struct Triangle {
	vec4 position;
	vec4 u;
	vec4 v;

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
	vec4 specular;
	vec4 emission_metallic;
};

uniform mat4 CAMERA;


layout(rgba32f, binding=0) restrict uniform image2D img_output;



layout(std430, binding=1) restrict readonly buffer visBuffer {
	int visibility[];
};


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
uvec2 SIZE;
ivec2 TEXEL;


vec3 random_hemi(vec3 n, uint step) {
	uint size = SIZE.x * SIZE.y * 4;
	vec3 rvec = rand_vec[((TEXEL.x * SIZE.y + TEXEL.y) * 13 + step + SAMPLE * 1123) % size].xyz;
	return dot(n, rvec) > 0 ? rvec : -rvec;
}

vec3 random_sphere(vec3 n, uint step) {
	uint size = SIZE.x * SIZE.y * 4;
	return rand_vec[((TEXEL.x * SIZE.y + TEXEL.y) * 17 + step + SAMPLE * 1123) % size].xyz;
}

vec3 intersect(vec3 rayPos, vec3 rayDir, vec3 p, vec3 u, vec3 v, vec3 N, float compDist, bool shadow) {
	vec3 relative;
	float rayDotN = dot(rayDir, N);
	if ((!shadow && rayDotN <= 0.0) || rayDotN == 0.0) {
	// if (rayDotN <= 0.0) {
		return vec3(0.0, 0.0, -1.0);
	}

	float Dinv = -1.0/rayDotN;
	vec3 delta = (rayPos-p)*Dinv;
	
	relative.z = dot(N, delta);
	if (relative.z < 0 || (compDist >= 0.0 && relative.z > compDist)) {
		return vec3(0.0, 0.0, -1.0);
	}
	
	vec3 subDet = cross(rayDir, delta);
	
	relative.x = dot(subDet, v);
	relative.y = -dot(subDet, u);
	
	if (relative.x >= 0.0 && relative.y >= 0.0 && relative.x + relative.y <= 1.0) {
		return relative;
	}
	return vec3(0.0, 0.0, -1.0);
}


bool hit(vec3 rayPos, vec3 rayDir, int avoid) {
	for (int triID = 0; triID < COUNT; ++triID) {
		if (triID == avoid) {
			continue;
		}

		Triangle tri = triangles[triID];
		vec3 sec = intersect(rayPos, rayDir, tri.position.xyz, tri.u.xyz, tri.v.xyz, cross(tri.v.xyz, tri.u.xyz), -1.0, true);
		if (sec.z > 0.0) {
			return true;
		}
	}
	return false;
}


vec3 trace(vec3 rayPos, vec3 rayDir) {
	vec3 final_color = vec3(0.0);
	vec3 path_color = vec3(0.0);

	vec3 prevSpecular = vec3(0.0);
	vec3 prevAlbedo = vec3(0.0);
	vec3 prevColor = vec3(1.0);
	float prevMetall = 0.0;
	float prevRoughness = 0.0;

	float intensity = 1.0;
	float prevLight = 0.0;
	
	int avoid_tri = -1;

	for (int step=0; step < max(RECURSION, 1); ++step) {
		vec3 current_intersection = vec3(0.0, 0.0, -1.0);
		int current_tri = -1;
		bool useVis = step > 0;
		for (int visID = 0; visID < COUNT; ++visID) {
			int triID = useVis ? visibility[avoid_tri * COUNT + visID] : visID;
			if (triID == avoid_tri) {
				break;
			}

			Triangle tri = triangles[triID];
			
			vec3 sec = intersect(rayPos, rayDir, tri.position.xyz, tri.u.xyz, tri.v.xyz, cross(tri.v.xyz, tri.u.xyz), current_intersection.z, false);
			if (sec.z > 0.0) {
				current_intersection = sec;
				current_tri = triID;
			}
		}
	
		if (current_tri >= 0) {
			Triangle tri = triangles[current_tri];
			vec3 sec = current_intersection;
			vec3 normal = normalize(tri.normal0.xyz * (1.0-sec.x-sec.y) + tri.normal1.xyz * sec.x + tri.normal2.xyz * sec.y);
			vec3 refl_ray_dir = reflect(rayDir, normal);

			Material material = materials[uint(ceil(tri.u.w))];

			vec3 alb = material.albedo.rgb;
			vec3 spc = material.specular.rgb;
			vec4 emi_met = material.emission_metallic;

			rayPos = rayPos + rayDir * sec.z;
			float light_intens = length(LIGHT_DIR);
			vec3 light_scatter = vec3(0.0);
			bool inShadow = true;
			float directLight = 0.0;
			float specDirectIntens = 0.0;

			if (light_intens > 0) {
				light_scatter = LIGHT_DIR + 0.0049*random_hemi(LIGHT_DIR, step*7+3);
				inShadow = hit(rayPos, -light_scatter, current_tri);
				directLight = inShadow ? 0.0 : max(-dot(normal, LIGHT_DIR), 0.0);
				specDirectIntens = inShadow ? 0.0 : max(-dot(refl_ray_dir, LIGHT_DIR), 0.0);
			}

			vec3 specularClr = specDirectIntens * spc;
			vec3 diffuseClr = (prevLight + directLight) * alb;
			vec3 emissionClr = emi_met.rgb;

			path_color += prevColor * (diffuseClr + emissionClr + specularClr) * intensity;
			if (dot(emissionClr, emissionClr) > 0 || !inShadow) {
				final_color += path_color;
				path_color = vec3(0.0);
			}
			prevColor = alb + spc;
			avoid_tri = current_tri;
			if (dot(prevColor, prevColor) == 0)
				break;

			intensity *= dot(LUMA, spc+alb);
			prevLight = max(-dot(rayDir, normal), 0.0);

			float roughness = dot(LUMA, alb);
			vec3 scatter = random_hemi(normal, step);
			prevSpecular = spc;
			prevMetall = emi_met.a;
			prevAlbedo = alb;

			if (roughness > 0)
				rayDir = normalize(mix(refl_ray_dir, scatter, roughness));
			else
				rayDir = refl_ray_dir;
		}
		else {
			break;
		}
	}
	if (avoid_tri >= 0)
		final_color += path_color*AMBIENT + (prevAlbedo + prevSpecular*prevLight)*AMBIENT*intensity;
	else
		final_color += AMBIENT;
	
	return final_color;
}

void main(void) {
	SIZE = imageSize(img_output);
	float inv_height = 1.0 / SIZE.y;
	TEXEL = ivec2(gl_GlobalInvocationID.xy);

	vec4 color = vec4(0.0, 0.0, 0.0, 1.0);
	Ray ray;
	ray.position = vec4(0, 0, 0, 1.0);
	vec3 dtctor = vec3((TEXEL.x - 0.5*SIZE.x)*inv_height, TEXEL.y*1.0f*inv_height - 0.5, 0.5);
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
