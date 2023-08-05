#version 450 core

layout (local_size_x=8, local_size_y=8, local_size_z=1) in;

struct Vec3 {
	float x, y, z;
};

struct Vec2 {
	float s, t;
};

struct Triangle {
	Vec3 position;
    Vec3 u;
    Vec3 v;

    Vec3 normals[3];
	Vec3 tangents[3];
    Vec3 true_normal;

    Vec2 tex_p;
    Vec2 tex_u;
    Vec2 tex_v;

	uint material_id;
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


uniform layout(rgba32f, binding=0) restrict image2D img_output;
uniform layout(location=1) sampler2D diff_tex;
uniform layout(location=2) sampler2D norm_tex;
uniform layout(location=3) sampler2D arm_tex;


layout(std430, binding=2) restrict readonly buffer rayBuffer {
	Ray rays[];
};


layout(std430, binding=3) restrict readonly buffer triangleBuffer {
	Triangle triangles[];
};

layout(std430, binding=4) restrict readonly buffer normalBuffer {
	vec3 normals[];
};

// Materials
layout(std430, binding=6) restrict readonly buffer materialBuffer {
	Material materials[];
};

uniform mat4 CAMERA;

uniform int COUNT;
uniform int RECURSION;
uniform uint SAMPLE;
uniform uint CLOCK;
uniform vec3 LIGHT_DIR;
uniform vec3 AMBIENT;

const vec3 LUMA = vec3(0.299, 0.587, 0.114);
const float PI = 3.141592653589793;
const float TWO_PI = 6.283185307179586;
const float INV_PI = 1.0/3.141592653589793;
uvec2 SIZE;
ivec2 TEXEL;

const uint A = 747796405u;
const uint B = 2891336453u;
const uint C = 277803737u;
const float INV_UINT_MAX = 1.0 / 0xFFFFFFFFU;


uint pcg_hash(in const uint k) {
    const uint state = k * A + B;
    const uint word = ((state >> ((state >> 28U) + 4U)) ^ state) * C;
    return (word >> 22U) ^ word;
}

float UnitFloat(inout uint seed) {
	seed = pcg_hash(seed);
	return seed * INV_UINT_MAX;
}

vec3 random_sphere(inout uint seed) {
	const float theta = TWO_PI * UnitFloat(seed);
	const float x = UnitFloat(seed) * 2.0 - 1.0;
	const float sinx = sqrt(1.0-x*x);

	return vec3(sinx*cos(theta), sinx*sin(theta), x);
}

vec3 random_hemi(in const vec3 n, inout uint seed) {
	const vec3 u = random_sphere(seed);
	return (dot(u, n) < 0) ? -u : u;
}


bool intersect(in const vec3 rayPos, in const vec3 rayDir,
			   in const vec3 triPos, in const vec3 u, in const vec3 v, in const vec3 n,
			   inout vec3 isec)
{
	const float determinant = -dot(rayDir, n);
	// Backface culling
	if (determinant <= 0.0)
		return false;

	const vec3 delta = rayPos - triPos;
	const float relative_depth = dot(n, delta);
	if (relative_depth <= 0)
		return false;
	
	// discard if the previous intersection is closer
	if (isec.z >= 0.0 && relative_depth > isec.z * determinant)
		return false;

	// some linear algebra to solve inverse(u, v, rayDir) * delta
	const vec3 subDet = cross(rayDir, delta);
	const vec3 relative = vec3(-dot(subDet, v), dot(subDet, u), relative_depth);

	if (relative.x <= 0.0 || relative.y <= 0.0 || relative.x + relative.y > determinant)
		return false;

	const float IDet = 1.0 / determinant;
	isec = relative * IDet;
	return true;
}

bool intersectShadow(in const vec3 rayPos, in const vec3 rayDir,
					 in const vec3 triPos, in const vec3 u, in const vec3 v, in const vec3 n)
{
	const float determinant = -dot(rayDir, n);
	// Backface culling
	if (determinant <= 0.0)
		return false;

	const vec3 delta = rayPos - triPos;
	const float relative_depth = dot(n, delta);
	if (relative_depth <= 0)
		return false;

	// some linear algebra to solve inverse(u, v, rayDir) * delta
	const vec3 subDet = cross(rayDir, delta);
	const vec3 relative = vec3(-dot(subDet, v), dot(subDet, u), relative_depth);

	if (relative.x <= 0.0 || relative.y <= 0.0 || relative.x + relative.y > determinant)
		return false;

	return true;
}

float getTransmission(in const float cosThetaI, in const float etaI, in const float etaT) {
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

int find_intersection(in vec3 rayPos, in vec3 rayDir, inout vec3 current_intersection) {
	int current_tri = -1;
	for (int triID = 0; triID < COUNT; triID++) {
		const Triangle tri = triangles[triID];

		const vec3 p = vec3(tri.position.x,tri.position.y,tri.position.z);
		const vec3 u = vec3(tri.u.x, tri.u.y, tri.u.z);
		const vec3 v = vec3(tri.v.x, tri.v.y, tri.v.z);
		const vec3 n = vec3(tri.true_normal.x,tri.true_normal.y,tri.true_normal.z);
		
		if (intersect(rayPos, rayDir, p, u, v, n, current_intersection))
			current_tri = triID;
	}
	return current_tri;
}

vec3 sRGBtoLinear(in vec3 C) {
	return pow((C + 0.055)/(1.055), vec3(2.4));
}

vec3 calculateN(in const Triangle tri, in const vec3 intersection) {
	const vec3 n0 = vec3(tri.normals[0].x,tri.normals[0].y,tri.normals[0].z);
	const vec3 n1 = vec3(tri.normals[1].x,tri.normals[1].y,tri.normals[1].z);
	const vec3 n2 = vec3(tri.normals[2].x,tri.normals[2].y,tri.normals[2].z);

	return normalize(n0 * (1.0-intersection.x-intersection.y) + n1 * intersection.x + n2 * intersection.y);
}

vec2 calculateUV(in const Triangle tri, in const vec3 intersection) {
	const vec2 tex_p = vec2(tri.tex_p.s, tri.tex_p.t);
	const vec2 tex_u = vec2(tri.tex_u.s, tri.tex_u.t);
	const vec2 tex_v = vec2(tri.tex_v.s, tri.tex_v.t);

	return tex_p + tex_u * intersection.x + tex_v * intersection.y;
}

mat3 calculateTBN(in const Triangle tri, in const vec3 N, in const vec3 intersection) {
	const vec3 t0 = vec3(tri.tangents[0].x,tri.tangents[0].y,tri.tangents[0].z);
	const vec3 t1 = vec3(tri.tangents[1].x,tri.tangents[1].y,tri.tangents[1].z);
	const vec3 t2 = vec3(tri.tangents[2].x,tri.tangents[2].y,tri.tangents[2].z);
	vec3 T = t0 * (1.0-intersection.x-intersection.y) + t1 * intersection.x + t2 * intersection.y;
	T = normalize(T - N * dot(N, T));

	return mat3(T, cross(T, N), N);
}

vec3 trace(in vec3 rayPos, in vec3 rayDir, uint seed) {
	vec3 energy = vec3(0.0);
	vec3 path = vec3(1.0);

	for (int step=0; step < max(RECURSION, 1); step++) { // max(RECURSION, 1)
		vec3 current_intersection = vec3(0.0, 0.0, -1.0);
		const int current_tri = find_intersection(rayPos, rayDir, current_intersection);

		if (current_tri < 0) {
			energy += path * mix(AMBIENT, vec3(1.0,0.99,0.97), length(LIGHT_DIR)*pow(max(0.0, dot(rayDir, normalize(LIGHT_DIR))), 8.0));
			//energy += path * (rayDir*0.5+0.5);
			break;
		}
	
		const Triangle tri = triangles[current_tri];
		const uint mat_id = tri.material_id;
		Material material = materials[mat_id];

		const vec3 sec = current_intersection;
		const vec2 tex_coord = calculateUV(tri, sec);
		const vec3 N = calculateN(tri, sec);
		const mat3 TBNi = calculateTBN(tri, N, sec);

		const vec3 tex_normal = normalize(texture(norm_tex, tex_coord).xyz*2.0-1.0);
		const vec3 normal = (mat_id == 2) ? normalize(TBNi * tex_normal) : N;
		const vec3 true_normal = vec3(tri.true_normal.x,tri.true_normal.y,tri.true_normal.z);

		const vec3 refl_ray_dir = reflect(rayDir, normal);

		const vec3 texel_albedo = sRGBtoLinear(texture(diff_tex, tex_coord).rgb);
		const vec3 texel_arm = texture(arm_tex, tex_coord).rgb;

		const vec3 albedo = (mat_id == 2) ? texel_albedo : material.albedo.rgb;
		const float scattering = (mat_id == 2) ? texel_arm.r : material.specular_roughness.a;

		const vec3 specular = material.specular_roughness.rgb;
		const vec3 emission = material.emission_ior.rgb;
		const float ior = material.emission_ior.a;

		rayPos += rayDir * sec.z;

		if (emission.r > 0 || emission.g > 0 || emission.b > 0) {
			energy += path * emission;
			path = vec3(1.0);
		}

		const float cosTheta = max(-dot(normal, rayDir), 0.0);
		
		const float transmission = (ior == 0.0) ? 0.0 : getTransmission(cosTheta, 1.00029, ior);

		const float mCosTheta = 1.0 - cosTheta;
		const float mCosTheta2 = mCosTheta * mCosTheta;
		const float mCosTheta4 = mCosTheta2 * mCosTheta2;
		const float mCosTheta5 = mCosTheta * mCosTheta4;

		const float fresnel = (1.0-transmission) * mCosTheta5;
		const float reflectance = transmission + fresnel;

		const float metallic = (mat_id == 2) ? texel_arm.b : float(scattering < 0.125);

		const float rand_reflectance = UnitFloat(seed);
		const float rand_scatter = UnitFloat(seed);
		const float rand_metallic = UnitFloat(seed);

		if (rand_reflectance < reflectance) {
			// Fresnel effect, total reflection
			path *= (rand_metallic <= metallic) ? vec3(1.0-scattering*scattering) : specular;
			rayDir = refl_ray_dir;
		}
		else {
			if (rand_metallic <= metallic) {
				// Metallic reflection with roughness
				const vec3 scatter_cone = normalize(random_sphere(seed)*scattering + refl_ray_dir);
				path *= albedo;
				rayDir = scatter_cone;
			}
			else {
				if (rand_scatter < scattering) {
					// Diffuse scattering
					const vec3 scatter_hemi = normalize(random_sphere(seed) + normal);

					// Oren-Nayar
					//*
					const float theta_i = acos(dot(rayDir, normal));
					const float theta_o = acos(dot(scatter_hemi, normal));
					const float variance = scattering * scattering;
					const float A = 1.0 - 0.5 * variance / (variance + 0.33);
					const float B = 0.45 * variance / (variance + 0.09);
					const float alpha = max(theta_i, theta_o);
					const float beta = min(theta_i, theta_o);

					path *= albedo * (A + B*max(0, theta_i-theta_o)*sin(alpha)*tan(beta)) * INV_PI * ((mat_id==2) ? texel_arm.r : 1.0);
					//*/
					
					// Lambertian
					//path *= albedo * INV_PI;
					rayDir = scatter_hemi; //mix(refl_ray_dir, scatter_hemi, scattering);
				}
				else {
					// Specular reflection
					path *= specular;
					rayDir = refl_ray_dir;
				}
			}
		}

		if (dot(rayDir, true_normal) <= 0)
			break;
		// rayPos += pow(0.5, 24) * vec3(tri.true_normal.x,tri.true_normal.y,tri.true_normal.z); //  pow(0.5, 24)
		/*
		const float brightness = max((path.r+path.g+path.b)/3.0, 1.0/32.0);
		if (UnitFloat(seed) > brightness)
			break;
		
		path /= brightness;
		//*/
	}
	
	return energy;
}

void main(void) {
	SIZE = imageSize(img_output);
	float inv_width = 1.0 / SIZE.x;
	float inv_height = 1.0 / SIZE.y;
	TEXEL = ivec2(gl_GlobalInvocationID.xy);
	const vec3 prev = imageLoad(img_output, TEXEL).rgb;

	uint seed = CLOCK + (TEXEL.x * SIZE.y + TEXEL.y + 394587U) * (SAMPLE+1U) * 13U + SAMPLE;

	vec4 color = vec4(0.0, 0.0, 0.0, 1.0);
	Ray ray;
	ray.position = vec4(0, 0, 0, 1.0);
	vec3 dtctor = vec3((TEXEL.x - 0.5*SIZE.x + 0.5)*inv_height, (TEXEL.y + 0.5)*1.0*inv_height - 0.5, 0.5) + 0.25*vec3(UnitFloat(seed)*inv_width, UnitFloat(seed)*inv_height, 0.0);
	ray.direction = vec4(normalize(dtctor - ray.position.xyz), 0.0);
	color.rgb = trace((CAMERA * ray.position).xyz, normalize(CAMERA * ray.direction).xyz, seed);

	if (SAMPLE > 0) {
		float inv_samples = 1.0 / (SAMPLE + 1U);
		float scale = SAMPLE * inv_samples;
		imageStore(img_output, TEXEL, vec4(prev * scale + inv_samples * color.rgb, 1.0));
	}
	else
		imageStore(img_output, TEXEL, vec4(color.rgb, 1.0));
}
