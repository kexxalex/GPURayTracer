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
    vec3 position;
    vec3 direction;
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

layout(std430, binding=4) restrict readonly buffer cummulateRelLightAreaBuffer {
    float cLightArea[];
};

// Materials
layout(std430, binding=6) restrict readonly buffer materialBuffer {
    Material materials[];
};

uniform layout(location = 4) mat4 CAMERA;

uniform layout(location = 5) int COUNT;
uniform layout(location = 6) int LIGHTS;
uniform layout(location = 7) int RECURSION;
uniform layout(location = 8) uint SAMPLE;
uniform layout(location = 9) uint CLOCK;
uniform layout(location = 10) vec3 LIGHT_DIR;
uniform layout(location = 11) vec3 AMBIENT;

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


bool intersectTriangle(in const Ray ray, in const Triangle triangle, inout vec3 isec)
{
    const vec3 true_normal = vec3(triangle.true_normal.x, triangle.true_normal.y, triangle.true_normal.z);
    const float determinant = -dot(ray.direction, true_normal);
    // Backface culling
    if (determinant <= 0.0)
        return false;

    const vec3 triPos = vec3(triangle.position.x, triangle.position.y, triangle.position.z);
    const vec3 delta = ray.position - triPos;
    const float relative_depth = dot(true_normal, delta);
    if (relative_depth <= 0.0)
        return false;
    
    // discard if the previous intersection is closer
    if (isec.z >= 0.0 && relative_depth > isec.z * determinant)
        return false;

    // some linear algebra to solve inverse(u, v, rayDir) * Delta
    const vec3 subDet = cross(ray.direction, delta);
    const vec3 triU = vec3(triangle.u.x, triangle.u.y, triangle.u.z);
    const vec3 triV = vec3(triangle.v.x, triangle.v.y, triangle.v.z);
    const vec3 relative = vec3(-dot(subDet, triV), dot(subDet, triU), relative_depth);

    if (relative.x < 0.0 || relative.y < 0.0 || relative.x + relative.y > determinant)
        return false;

    const float IDet = 1.0 / determinant;
    isec = relative * IDet;
    return true;
}

bool intersectTriangleShadow(in const Ray ray, in const Triangle triangle, in float max_t)
{
    const vec3 normal = vec3(triangle.true_normal.x, triangle.true_normal.y, triangle.true_normal.z);
    const float determinant = dot(ray.direction, normal);
    // Backface culling
    if (determinant <= 0.0)
        return false;

    const vec3 triPos = vec3(triangle.position.x, triangle.position.y, triangle.position.z);
    const vec3 delta = triPos - ray.position;
    const float relative_depth = dot(normal, delta);
    if (relative_depth <= 0)
        return false;
    
    // discard if the previous intersection is closer
    if (relative_depth > determinant * max_t)
        return false;

    // some linear algebra to solve inverse(u, v, ray.direction) * Delta
    const vec3 subDet = cross(ray.direction, delta);
    const vec3 triU = vec3(triangle.u.x, triangle.u.y, triangle.u.z);
    const vec3 triV = vec3(triangle.v.x, triangle.v.y, triangle.v.z);
    const vec2 relative = vec2(-dot(subDet, triV), dot(subDet, triU));

    if (relative.x < 0.0 || relative.y < 0.0 || relative.x + relative.y > determinant)
        return false;
    return true;
}

float getTransmission(in const float cosThetaI, in const float etaI, in const float etaT)
{
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

int find_intersection(in const Ray ray, inout vec3 current_intersection)
{
    int current_tri = -1;
    for (int triID = 0; triID < COUNT; triID++)
    {
        if (intersectTriangle(ray, triangles[triID], current_intersection))
            current_tri = triID;
    }
    return current_tri;
}

vec3 sRGBtoLinear(in vec3 C) { return pow((C + 0.055)/1.055, vec3(2.4)); }

vec3 calculateN(in const Triangle tri, in const vec3 intersection)
{
    const mat3 N = mat3(
        tri.normals[0].x, tri.normals[0].y, tri.normals[0].z,
        tri.normals[1].x, tri.normals[1].y, tri.normals[1].z,
        tri.normals[2].x, tri.normals[2].y, tri.normals[2].z
    );

    return normalize(N * vec3(1.0-intersection.x-intersection.y, intersection.xy));
}

vec2 calculateUV(in const Triangle tri, in const vec3 intersection)
{
    const mat3x2 tex = mat3x2(
        tri.tex_p.s, tri.tex_p.t,
        tri.tex_u.s, tri.tex_u.t,
        tri.tex_v.s, tri.tex_v.t
    );

    return tex * vec3(1.0, intersection.xy);
}

mat3 calculateTBN(in const Triangle tri, in const vec3 N, in const vec3 intersection)
{
    const vec3 t0 = vec3(tri.tangents[0].x,tri.tangents[0].y,tri.tangents[0].z);
    const vec3 t1 = vec3(tri.tangents[1].x,tri.tangents[1].y,tri.tangents[1].z);
    const vec3 t2 = vec3(tri.tangents[2].x,tri.tangents[2].y,tri.tangents[2].z);
    vec3 T = t0 * (1.0-intersection.x-intersection.y) + t1 * intersection.x + t2 * intersection.y;
    T = normalize(T - N * dot(N, T));

    return mat3(T, cross(T, N), N);
}

bool sampleLight(in const vec3 position, in const vec3 normal, in const int current, inout uint seed, out vec3 light)
{
    if (LIGHTS == 0)
        return false;

    bool anyLight = false;
    light = vec3(0.0);

    for (int i=0; i < 1; i++) {
        const int lightID = int(UnitFloat(seed) * LIGHTS);
        const Triangle lightTri = triangles[lightID];
        const vec3 N = vec3(lightTri.true_normal.x,lightTri.true_normal.y,lightTri.true_normal.z);

        const mat3 UVP = mat3(
            lightTri.u.x, lightTri.u.y, lightTri.u.z,
            lightTri.v.x, lightTri.v.y, lightTri.v.z,
            lightTri.position.x,lightTri.position.y,lightTri.position.z
        );

        /* Uniform triangle distribution
        1. uniformly distributed square: [0, 1) x [0, 1)
        2. reflect point outside the lower left triangle through (0.5, 0.5) to its inside: {(x,y) | x+y < 1 ; x,y >= 0}
        3. use these as coefficients for the span vectors of the triangle
        */
        vec2 coords = vec2(UnitFloat(seed), UnitFloat(seed));
        if (coords.x+coords.y > 1)
            coords = 1.0 - coords;

        const vec3 rand_tri = UVP * vec3(coords, 1.0);
        const vec3 delta = rand_tri-position;
        const float max_t = length(delta);

        Ray lightProbeRay;
        lightProbeRay.position = position;
        lightProbeRay.direction = delta / max_t;

        if (dot(lightProbeRay.direction, normal) <= 0)
            continue;

        bool inLight = true;
        
        for (int triID = 0; triID < COUNT; triID++)
        {
            if (triID == current)
                continue;
            if (intersectTriangleShadow(lightProbeRay, triangles[triID], max_t)) {
                inLight = false;
                break;
            }
        }
        if (inLight) {
            anyLight = true;
            const Material material = materials[lightTri.material_id];
            // const float triArea = 0.5 * length(cross(UVP[0], UVP[1]));
            light += material.emission_ior.rgb * (max(-dot(lightProbeRay.direction, N), 0.0) * dot(lightProbeRay.direction, normal) / (max_t*max_t + 2.0 * max_t + 1.0) * LIGHTS);
        }
    }
    return anyLight;
}


bool sampleLightGlossy(in const vec3 position, in const vec3 normal, in const Ray view, in const float roughness, in const int current, inout uint seed, out vec3 light)
{
    if (LIGHTS == 0)
        return false;

    const vec3 viewDir = normalize(position - view.position);
    Ray lightProbeRay;
    lightProbeRay.position = position;
    lightProbeRay.direction = reflect(viewDir, normalize(random_sphere(seed)*roughness + normal));

    vec3 intersection = vec3(0.0, 0.0, -1.0);
    int lightID = -1;
    
    for (int triID = 0; triID < COUNT; triID++)
    {
        if (triID == current)
            continue;
        if (intersectTriangle(lightProbeRay, triangles[triID], intersection)) {
            lightID = triID;
            if (lightID >= LIGHTS)
                return false;
        }
    }
    if (lightID < 0)
        return false;

    const Triangle lightTri = triangles[lightID];
    const Material material = materials[lightTri.material_id];
    const vec3 N = calculateN(lightTri, intersection);
    /*
    const float dist = intersection.z;
    const float dist2 = dist*dist;

    float area = 0.5*length(cross(
        vec3(lightTri.u.x, lightTri.u.y, lightTri.u.z),
        vec3(lightTri.v.x, lightTri.v.y, lightTri.v.z)
    ));
    */

    light = material.emission_ior.rgb * max(-dot(lightProbeRay.direction, N), 0.0);// / (dist2 + 2.0 * dist + 1.0);
    return true;
}

vec3 trace(in Ray ray, in uint seed) {
    vec3 energy = vec3(0.0);
    vec3 path = vec3(1.0);

    const int MAX_RECURSION = max(RECURSION, 1);
    for (int depth=0; depth < MAX_RECURSION; depth++) {
        vec3 current_intersection = vec3(0.0, 0.0, -1.0);
        const int current_tri = find_intersection(ray, current_intersection);

        if (current_tri < 0)
        {
            energy += (AMBIENT + vec3(1.0,0.96,0.9) * length(LIGHT_DIR)*pow(max(0.0, dot(ray.direction, normalize(LIGHT_DIR))), 5.0)) * path;
            break;
        }
    
        const Triangle tri = triangles[current_tri];
        const uint mat_id = tri.material_id;
        const Material material = materials[mat_id];

        if (current_tri < LIGHTS)
        {
            if (depth == 0)
                energy = material.emission_ior.rgb;
            break;
        }

        const vec3 sec = current_intersection;
        const vec2 tex_coord = calculateUV(tri, sec);
        const vec3 N = calculateN(tri, sec);
        const mat3 TBNi = calculateTBN(tri, N, sec);

        const vec3 texel_albedo = sRGBtoLinear(texture(diff_tex, tex_coord).rgb);
        const vec3 tex_normal = normalize(texture(norm_tex, tex_coord).xyz*2.0-1.0);
        const vec3 texel_arm = texture(arm_tex, tex_coord).rgb;

        const vec3 normal = (mat_id == 2) ? normalize(TBNi * tex_normal) : N;
        const vec3 true_normal = vec3(tri.true_normal.x,tri.true_normal.y,tri.true_normal.z);

        const vec3 albedo = (mat_id == 2) ? texel_albedo : material.albedo.rgb;
        const vec3 specular = material.specular_roughness.rgb;
        const float ior = material.emission_ior.a;

        const float scattering = (mat_id == 2) ? texel_arm.g : material.specular_roughness.a;
        const float metallic = (mat_id == 2) ? texel_arm.b : float(scattering < 0.125);

        const vec3 specular_ray = normalize(reflect(ray.direction, normal));
        const vec3 scattered_specular_ray = normalize(random_sphere(seed)*scattering + specular_ray);
        const vec3 scattered_diffuse = normalize(random_sphere(seed) + normal);
        const vec3 scattered_spec_diff_ray = normalize(random_sphere(seed)*scattering + normal);

        const float cosTheta = max(-dot(normal, ray.direction), 0.0);
        
        const float transmission = (ior == 0.0) ? 0.0 : getTransmission(cosTheta, 1.00029, ior);
        const float reflectance = fma(1.0-transmission, pow(1.0-cosTheta, 5), transmission);

        const float rand_reflectance = UnitFloat(seed);
        const float rand_scatter = UnitFloat(seed);
        const float rand_metallic = UnitFloat(seed);

        Ray oldRay;
        oldRay.position = ray.position;
        oldRay.direction = ray.direction;

        ray.position += ray.direction * sec.z;

        bool isGlossy = true;

        if (rand_reflectance < reflectance) {
            // Fresnel effect, total reflection
            path *= (rand_metallic < metallic) ? vec3(1.0-scattering*scattering) : specular;
            ray.direction = specular_ray;
            //isGlossy = (rand_metallic < metallic);
        }
        else {
            if (rand_metallic < metallic) {
                // Metallic reflection with roughness
                path *= albedo;
                ray.direction = scattered_specular_ray;
            }
            else {
                if (rand_scatter < scattering) {
                    // Oren-Nayar
                    const float theta_i = acos(cosTheta);
                    const float theta_o = acos(dot(scattered_diffuse, normal));
                    const float variance = scattering * scattering;
                    const float A = 1.0 - 0.5 * variance / (variance + 0.33);
                    const float B = 0.45 * variance / (variance + 0.09);
                    const float alpha = max(theta_i, theta_o);
                    const float beta = min(theta_i, theta_o);

                    path *= albedo * ((A + B*max(0, theta_i-theta_o)*sin(alpha)*tan(beta)) * INV_PI);
                    ray.direction = scattered_diffuse;
                    isGlossy = false;
                }
                else {
                    // Specular reflection
                    path *= specular;
                    ray.direction = scattered_spec_diff_ray;
                }
            }
        }

        vec3 globalLight = vec3(0.0);
        bool hasLight = false;
        const float rand_light = UnitFloat(seed) * (1.0 + reflectance);
        if (rand_light < scattering)
            hasLight = sampleLight(ray.position, normal, current_tri, seed, globalLight);
        else
            hasLight = sampleLightGlossy(ray.position, normal, oldRay, scattering, current_tri, seed, globalLight);


        if (hasLight)
            energy += path * globalLight;

        if (dot(ray.direction, true_normal) <= 0)
            break;
        //*
        // Russian roulette
        float brightness = max((hasLight ? dot(globalLight, path) : dot(vec3(1.0), path))/3.0, 1.0/16.0);
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
    vec4 position = vec4(0.0, 0.0, 0.0, 1.0);
    vec3 dtctor = vec3((TEXEL.x - 0.5*SIZE.x + 0.5)*inv_height, (TEXEL.y + 0.5)*1.0*inv_height - 0.5, 0.5) + 0.25*vec3(UnitFloat(seed)*inv_width, UnitFloat(seed)*inv_height, 0.0);
    vec4 direction = vec4(normalize(dtctor - position.xyz), 0.0);
    Ray ray;
    ray.position = (CAMERA * position).xyz;
    ray.direction = normalize(CAMERA * direction).xyz;
    color.rgb = trace(ray, seed);

    if (SAMPLE > 0) {
        float inv_samples = 1.0 / (SAMPLE + 1U);
        float scale = SAMPLE * inv_samples;
        imageStore(img_output, TEXEL, vec4(prev * scale + inv_samples * color.rgb, 1.0));
    }
    else
        imageStore(img_output, TEXEL, vec4(color.rgb, 1.0));
}
