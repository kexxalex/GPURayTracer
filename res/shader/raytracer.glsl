#version 450 core

layout (local_size_x=8, local_size_y=8, local_size_z=1) in;

struct Vec3 {
    float x, y, z;
};

struct Vec2 {
    float s, t;
};

struct TriangleModel {
    Vec3 true_normal;
    Vec3 position;
    Vec3 u;
    Vec3 v;
};

struct TriangleShading {
    uint material_id;

    Vec3 normals[3];
    Vec3 tangents[3];

    Vec2 tex_p;
    Vec2 tex_u;
    Vec2 tex_v;
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
uniform layout(location=1) sampler2DArray textureAtlas;
uniform layout(location=2) sampler2D sky_tex;
uniform layout(location=13) float EXPOSURE;


layout(std430, binding=1) restrict readonly buffer triangleModelBuffer {
    TriangleModel triangleModels[];
};

layout(std430, binding=2) restrict readonly buffer triangleShadingBuffer {
    TriangleShading triangleShadings[];
};

layout(std430, binding=3) restrict readonly buffer materialBuffer {
    Material materials[];
};

layout(std430, binding=6) restrict readonly buffer hasTextureBuffer {
    int hasTexture[];
};

uniform layout(location = 4) mat4 CAMERA;

uniform layout(location = 5) int COUNT;
uniform layout(location = 6) int LIGHTS;
uniform layout(location = 7) int RECURSION;
uniform layout(location = 8) uint SAMPLE;
uniform layout(location = 9) uint CLOCK;

uniform layout(location = 14) vec3 BB_CENTER;
uniform layout(location = 15) int SPLIT_X;

const float PI = 3.141592653589793;
const float TWO_PI = 6.283185307179586;
const float INV_PI = 1.0/3.141592653589793;
uvec2 SIZE;
ivec2 TEXEL;

const uint A = 747796405u;
const uint B = 2891336453u;
const uint C = 277803737u;
const float INV_UINT_MAX = 1.0 / 0xFFFFFFFFu;


uint pcgHash(in const uint k) {
    const uint state = k * A + B;
    const uint word = ((state >> ((state >> 28U) + 4U)) ^ state) * C;
    return (word >> 22U) ^ word;
}

float unitFloat(inout uint seed) {
    seed = pcgHash(seed);
    return seed * INV_UINT_MAX;
}

vec3 randomSphere(inout uint seed)
{
    /*
        Uniformly distributed point on the boundary of a sphere with radius 1.
        This means, for any chosen 0 < area < 4pi,
        the position of the area on the sphere does not change the density.
        (Rotational symmetry)
    */
    const float theta = TWO_PI * unitFloat(seed);
    const float x = unitFloat(seed) * 2.0 - 1.0;
    const float sinx = sqrt(1.0-x*x);

    return vec3(sinx * cos(theta), sinx * sin(theta), x);
}

vec3 randomHemi(in const vec3 n, inout uint seed)
{
    /*
        Uniformly distributed point on the upper hemisphere with radius 1 with maximum in direction n.
    */
    const vec3 u = randomSphere(seed);
    return (dot(u, n) < 0) ? -u : u;
}


bool intersectTriangle(in const Ray ray, in const TriangleModel triangle, inout vec4 isec)
{
    /*
        Calculates the relative intersection point in (triangle.u, triangle.v, ray.direction) [local] space.
        If the triangle is facing away from the ray, it's discarded
        If the triangle is behind the ray starting position => discarded
        If the distance is larger than that stored in isec.z (and isec.z >= 0) => discarded
        Only if none of the previous discard criterias are met and the relative coordinates lie in the triangle,
        isec is overwritten for further intersections or reflections.
    */
    const vec3 true_normal = vec3(triangle.true_normal.x, triangle.true_normal.y, triangle.true_normal.z);
    const float determinant = -dot(ray.direction, true_normal);
    // Backface culling
    if (determinant <= 0.0)
        return false;

    const vec3 tri_pos = vec3(triangle.position.x, triangle.position.y, triangle.position.z);
    const vec3 delta = ray.position - tri_pos;
    const float relative_depth = dot(true_normal, delta);
    if (relative_depth <= 0.0)
        return false;
    
    // discard if the previous intersection is closer
    if (isec.z >= 0.0 && relative_depth*isec.w > isec.z * determinant)
        return false;

    // some linear algebra to solve inverse(u, v, rayDir) * Delta
    const vec3 minor = cross(ray.direction, delta);
    const vec3 tri_u = vec3(triangle.u.x, triangle.u.y, triangle.u.z);
    const vec3 tri_v = vec3(triangle.v.x, triangle.v.y, triangle.v.z);
    const vec4 relative = vec4(-dot(minor, tri_v), dot(minor, tri_u), relative_depth, determinant);

    if (relative.x < 0.0 || relative.y < 0.0 || relative.x + relative.y > determinant)
        return false;

    isec = relative;
    return true;
}

bool intersectTriangleShadow(in const Ray ray, in const TriangleModel triangle, in float max_t)
{
    /*
        Unlike the intersectTriangle, these function discards front facing triangles
        and does not calculate the relative coordinates in local space.
        All triangles farther away than max_t are discarded, can be used as precalculated light source distance.
    */
    const vec3 true_normal = vec3(triangle.true_normal.x, triangle.true_normal.y, triangle.true_normal.z);
    const float determinant = dot(ray.direction, true_normal);
    // Frontface culling
    if (determinant <= 0.0)
        return false;

    const vec3 tri_pos = vec3(triangle.position.x, triangle.position.y, triangle.position.z);
    const vec3 delta = tri_pos - ray.position;
    const float relative_depth = dot(true_normal, delta);
    if (relative_depth <= 0.0)
        return false;
    
    // discard if the previous intersection is closer
    if (relative_depth > determinant * max_t)
        return false;

    // some linear algebra to solve inverse(u, v, ray.direction) * Delta
    const vec3 minor = cross(ray.direction, delta);
    const vec3 tri_u = vec3(triangle.u.x, triangle.u.y, triangle.u.z);
    const vec3 tri_v = vec3(triangle.v.x, triangle.v.y, triangle.v.z);
    const vec2 relative = vec2(-dot(minor, tri_v), dot(minor, tri_u));

    if (relative.x < 0.0 || relative.y < 0.0 || relative.x + relative.y > determinant)
        return false;
    return true;
}

float getTransmission(in const float cosThetaI, in const float etaI, in const float etaT)
{
    /*
        Some physics to calculate the transmission coefficient.
        It holds T + R = 1 (energy conservation)
    */
    const float sinThetaI = sqrt(1.0 - cosThetaI*cosThetaI);
    const float sinThetaT = etaI / etaT * sinThetaI;
    if (sinThetaT >= 1.0)
        return 1.0;
    
    const float cosThetaT = sqrt(1.0 - sinThetaT*sinThetaT);
    vec2 reflectance = vec2(
        (etaT * cosThetaI - etaI * cosThetaT) / (etaT * cosThetaI + etaI * cosThetaT),
        (etaI * cosThetaI - etaT * cosThetaT) / (etaI * cosThetaI + etaT * cosThetaT)
    );
    return dot(reflectance, reflectance) * 0.5;
}

int findIntersection(in const Ray ray, out vec3 current_intersection)
{
    /*
        Iterates all triangles (the light sources included) and returns the ID of the closest.
        The local coordinates are stored in current_intersection
    */
    int current_tri = -1;
    vec4 intersection = vec4(0.0, 0.0, -1.0, 1.0);
    for (int triID = 0; triID < COUNT; triID++)
    {
        if (intersectTriangle(ray, triangleModels[triID], intersection))
            current_tri = triID;
    }

    const float inv_det = 1.0 / intersection.w;
    current_intersection = intersection.xyz * inv_det;

    return current_tri;
}

vec3 sRGBtoLinear(in vec3 C) { return pow((C + 0.055)/1.055, vec3(2.4)); }

vec3 calculateN(in const TriangleShading tri, in const vec3 intersection)
{
    const mat3 N = mat3(
        tri.normals[0].x, tri.normals[0].y, tri.normals[0].z,
        tri.normals[1].x, tri.normals[1].y, tri.normals[1].z,
        tri.normals[2].x, tri.normals[2].y, tri.normals[2].z
    );

    return normalize(N * vec3(1.0-intersection.x-intersection.y, intersection.xy));
}

vec2 calculateUV(in const TriangleShading tri, in const vec3 intersection)
{
    const mat3x2 tex = mat3x2(
        tri.tex_p.s, tri.tex_p.t,
        tri.tex_u.s, tri.tex_u.t,
        tri.tex_v.s, tri.tex_v.t
    );

    return tex * vec3(1.0, intersection.xy);
}

mat3 calculateTBN(in const TriangleShading tri, in const vec3 N, in const vec3 intersection)
{
    const vec3 t0 = vec3(tri.tangents[0].x,tri.tangents[0].y,tri.tangents[0].z);
    const vec3 t1 = vec3(tri.tangents[1].x,tri.tangents[1].y,tri.tangents[1].z);
    const vec3 t2 = vec3(tri.tangents[2].x,tri.tangents[2].y,tri.tangents[2].z);
    vec3 T = t0 * (1.0-intersection.x-intersection.y) + t1 * intersection.x + t2 * intersection.y;
    T = normalize(T - N * dot(N, T));

    return mat3(T, cross(T, N), N);
}


vec3 skyColor(in const vec3 direction) {
    const vec2 uv = vec2(atan(direction.z, direction.x) * INV_PI * 0.5, -asin(direction.y) * INV_PI) + vec2(0.5);
    return texture(sky_tex, uv).rgb * EXPOSURE;
}


bool sampleSky(in const vec3 position, in const vec3 normal,
               in const int current, inout uint seed, out vec3 light)
{
    Ray light_probe_ray;
    light_probe_ray.position = position;
    light_probe_ray.direction = normalize(randomSphere(seed) + normal);

    vec4 intersection = vec4(0.0, 0.0, -1.0, 0.0);

    for (int triID = 0; triID < COUNT; triID++)
    {
        if (triID == current)
            continue;
        if (intersectTriangle(light_probe_ray, triangleModels[triID], intersection))
            return false;
    }

    light = skyColor(light_probe_ray.direction);
    return true;
}

bool sampleLight(in const vec3 position, in const vec3 normal,
                 in const int current, inout uint seed, out vec3 light)
{
    /*
        Samples one random light source as representation for all sources present,
        assuming diffuse behaviour of the material.
    */
    const int light_id = int(unitFloat(seed) * LIGHTS + 1);
    if (light_id == 0)
    {
        return sampleSky(position, normal, current, seed, light);
    }

    const TriangleModel light_tri = triangleModels[light_id-1];

    const mat3 UVP = mat3(
        light_tri.u.x, light_tri.u.y, light_tri.u.z,
        light_tri.v.x, light_tri.v.y, light_tri.v.z,
        light_tri.position.x, light_tri.position.y, light_tri.position.z
    );

    /*
    Uniform triangle distribution
        1. uniformly distributed square: [0, 1) x [0, 1)
        2. reflect point outside the lower left triangle through (0.5, 0.5) to its inside: {(x,y) | x+y < 1 ; x,y >= 0}
        3. use these as coefficients for the span vectors of the triangle
    */
    vec2 coords = vec2(unitFloat(seed), unitFloat(seed));
    if (coords.x + coords.y > 1)
        coords = 1.0 - coords;

    const vec3 rand_tri = UVP * vec3(coords, 1.0);
    const vec3 delta = rand_tri - position;
    const float max_t = length(delta);

    Ray light_probe_ray;
    light_probe_ray.position = position;
    light_probe_ray.direction = delta / max_t;

    // Sampled light source is behind the point, therefore it cannot be lit.

    const vec3 true_normal = vec3(light_tri.true_normal.x, light_tri.true_normal.y, light_tri.true_normal.z);
    const float DdN = dot(light_probe_ray.direction, normal);
    const float DdL = dot(light_probe_ray.direction, true_normal);
    if (DdN <= 0.0 || DdL >= 0.0)
        return false;
        
    for (int triID = 0; triID < COUNT; triID++)
    {
        if (triID == current)
            continue;
        if (intersectTriangleShadow(light_probe_ray, triangleModels[triID], max_t))
            return false;
    }
    const Material material = materials[triangleShadings[light_id].material_id];

    const float tri_area = 0.5 * length(cross(UVP[0], UVP[1]));

    const float light_vis_area_ratio = -DdL; //-dot(true_normal, normal);
    const float probe_ratio = DdN;
    const float distance_ratio = fma(max_t, max_t + 2.0, 1.0);
    light = material.emission_ior.rgb * (tri_area * light_vis_area_ratio * probe_ratio / distance_ratio * LIGHTS);
    return true;
}


bool sampleLightGlossy(in const vec3 position, in const vec3 normal, in const Ray view,
                       in const float roughness, in const int current, inout uint seed, out vec3 light)
{
    /*
        Calculates, how much light in sampled glossy direction is incoming if the point is viewed from view
    */

    const vec3 viewDir = normalize(position - view.position);
    Ray light_probe_ray;
    light_probe_ray.position = position;
    light_probe_ray.direction = reflect(viewDir, normalize(randomSphere(seed)*roughness + normal));

    vec4 intersection = vec4(0.0, 0.0, -1.0, 0.0);
    int lightID = -1;
    
    for (int triID = 0; triID < COUNT; triID++)
    {
        if (triID == current)
            continue;
        if (intersectTriangle(light_probe_ray, triangleModels[triID], intersection)) {
            if (triID < LIGHTS) {
                lightID = triID;
            }
            else {
                lightID = -1;
                break;
            }
        }
    }
    if (lightID < 0)
    {
        if (intersection.z < 0.0)
            light = skyColor(light_probe_ray.direction);
        return (intersection.z < 0.0);
    }
    
    const float inv_det = 1.0 / intersection.w;
    intersection.xyz *= inv_det;

    const TriangleShading light_tri = triangleShadings[lightID];
    const Material material = materials[light_tri.material_id];
    const vec3 N = calculateN(light_tri, intersection.xyz);

    const float d = intersection.z;

    light = material.emission_ior.rgb * max(dot(light_probe_ray.direction, N), 0.0);// / fma(d, d+2.0, 1.0));
    return true;
}

vec3 trace(in Ray ray, in uint seed, in uint light_seed) {
    vec3 energy = vec3(0.0);
    vec3 path = vec3(1.0);

    const int MAX_RECURSION = max(RECURSION, 1);
    for (int depth=0; depth < MAX_RECURSION; depth++)
    {
        vec3 current_intersection;
        const int current_tri = findIntersection(ray, current_intersection);

        if (current_tri < 0)
        {
            energy += skyColor(ray.direction) * path;
            break;
        }
    
        const TriangleShading tri = triangleShadings[current_tri];
        const uint mat_id = tri.material_id;
        const Material material = materials[mat_id];
        const bool has_texture = hasTexture[mat_id] == 1;

        if (current_tri < LIGHTS)
        {
            energy += path * material.emission_ior.rgb;
        }

        const vec3 sec = current_intersection;
        const vec3 N = calculateN(tri, sec);
        
        const vec2 tex_coord = calculateUV(tri, sec);
        const mat3 TBNi = calculateTBN(tri, N, sec);

        const vec3 texel_albedo = sRGBtoLinear(texture(textureAtlas, vec3(tex_coord, mat_id*3 + 0)).rgb);
        const vec3 tex_normal = normalize(texture(textureAtlas, vec3(tex_coord, mat_id*3 + 1)).xyz*2.0-1.0);
        const vec3 texel_arm = texture(textureAtlas, vec3(tex_coord, mat_id*3 + 2)).rgb;

        const vec3 normal = (has_texture) ? normalize(TBNi * tex_normal) : N;

        const vec3 albedo = (has_texture) ? texel_albedo : material.albedo.rgb;
        const vec3 specular = (has_texture) ? texel_albedo : material.specular_roughness.rgb;
        const float ior = material.emission_ior.a;

        const float occlusion = (has_texture) ? texel_arm.r : 1.0;
        const float roughness = (has_texture) ? texel_arm.g : material.specular_roughness.a;
        const float variance = roughness * roughness;
        const float metallic = (has_texture) ? texel_arm.b : float(roughness < 0.125);

        const vec3 specular_ray           = normalize(reflect(ray.direction, normal));
        const vec3 scattered_specular_ray = normalize(randomSphere(seed)*variance + specular_ray);
        const vec3 scattered_diffuse      = normalize(randomSphere(seed) + normal);
        const vec3 scattered_glossy_ray   = normalize(randomSphere(seed)*variance + normal);

        const float cos_theta = max(-dot(normal, ray.direction), 0.0);
        
        const float transmission = (ior == 0.0) ? 0.0 : getTransmission(cos_theta, 1.00029, ior);
        const float fresnel_reflectance = fma(1.0-transmission, pow(1.0-cos_theta, 5), transmission);

        const float rand_reflectance = unitFloat(seed);
        const float rand_scatter = unitFloat(seed);
        const float rand_metallic = unitFloat(seed);

        Ray old_ray;
        old_ray.position = ray.position;
        old_ray.direction = ray.direction;

        ray.position += ray.direction * sec.z;

        if (rand_reflectance < fresnel_reflectance) {
            // Fresnel effect, total reflection
            path *= (rand_metallic < metallic) ? vec3(1.0-roughness*roughness) : specular;
            ray.direction = specular_ray;
        }
        else {
            if (rand_metallic < metallic)
            {
                // Metallic reflection with roughness
                path *= albedo;
                ray.direction = scattered_specular_ray;
            }
            else {
                if (rand_scatter < roughness)
                {
                    // Oren-Nayar
                    const float theta_i = acos(cos_theta);
                    const float theta_o = acos(dot(scattered_diffuse, normal));
                    const float variance = roughness * roughness;
                    const float A = 1.0 - 0.5 * variance / (variance + 0.33);
                    const float B = 0.45 * variance / (variance + 0.09);
                    const float alpha = max(theta_i, theta_o);
                    const float beta = min(theta_i, theta_o);

                    path *= albedo * ((A + B*max(0, theta_i-theta_o)*sin(alpha)*tan(beta)) * INV_PI) * occlusion;
                    ray.direction = scattered_diffuse;
                }
                else
                {
                    // Glossy reflection
                    path *= specular * (1.0-roughness);
                    ray.direction = scattered_glossy_ray;
                }
            }
        }

        vec3 diff_light = vec3(0.0);
        vec3 spec_light = vec3(0.0);
        /*
        bool has_light = false;
        if (unitFloat(light_seed) < 0.5)
            has_light = sampleLight(ray.position, normal, current_tri, seed, diff_light);
        else
            has_light = sampleLightGlossy(ray.position, normal, old_ray, roughness, current_tri, seed, spec_light);
        const vec3 global_light = (albedo * diff_light * roughness + specular * spec_light * (fresnel_reflectance + 1.0 - roughness)) * 2.0;
        /*/
        bool has_light = sampleLight(ray.position, normal, current_tri, seed, diff_light);
        bool has_spec_light = sampleLightGlossy(ray.position, normal, old_ray, roughness, current_tri, seed, spec_light);
        has_light = has_light || has_spec_light;
        const vec3 global_light = albedo * diff_light * roughness + specular * spec_light * (fresnel_reflectance + 1.0 - roughness);
        //*/

        if (has_light)
            energy += path * global_light;

        const TriangleModel tri_model = triangleModels[current_tri];
        const vec3 true_normal = vec3(tri_model.true_normal.x, tri_model.true_normal.y, tri_model.true_normal.z);
        if (dot(ray.direction, true_normal) <= 0.0)
            break;

        //*
        // Russian roulette
        float brightness = clamp(max(dot(global_light, path), (path.r+path.g+path.b) / 3.0), 1.0/16.0, 1.0);
        if (unitFloat(seed) > brightness)
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
    if (TEXEL.x >= SIZE.x || TEXEL.y >=  SIZE.y)
        return;

    uint seed = CLOCK + (TEXEL.x * SIZE.y + TEXEL.y + 394587U) * (SAMPLE+1U) * 13U + SAMPLE;
    uint light_seed = CLOCK;
    for (int i=0; i < SAMPLE; i++) {
        light_seed = pcgHash(light_seed);
    }

    vec4 color = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 position = vec4(0.0, 0.0, 0.0, 1.0);
    vec3 dtctor = normalize(vec3((TEXEL.x - 0.5*SIZE.x + 0.5)*inv_height, (TEXEL.y + 0.5)*inv_height - 0.5, 0.5) + vec3(unitFloat(seed)*inv_width, unitFloat(seed)*inv_height, 0.0));
    vec4 direction = vec4(normalize(dtctor - position.xyz), 0.0);
    Ray ray;
    ray.position = (CAMERA * position).xyz;
    ray.direction = normalize(CAMERA * direction).xyz;
    color.rgb = trace(ray, seed, light_seed);

    if (SAMPLE > 0) {
        const vec3 prev = imageLoad(img_output, TEXEL).rgb;
        float inv_samples = 1.0 / (SAMPLE + 1U);
        float scale = SAMPLE * inv_samples;
        imageStore(img_output, TEXEL, vec4(prev * scale + inv_samples * color.rgb, 1.0));
    }
    else
        imageStore(img_output, TEXEL, vec4(color.rgb, 1.0));
}
