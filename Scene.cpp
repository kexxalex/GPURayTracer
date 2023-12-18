#include <iostream>
#include <fstream>
#include <algorithm>
#include "Scene.hpp"
#include "Shader.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <zlib.h>
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 0
#define TINYEXR_IMPLEMENTATION
#include "tinyexr/tinyexr.h"


#ifdef __linux__
#define SSCANF sscanf
#include <GLFW/glfw3.h>
#elif _WIN32
#define SSCANF sscanf_s
#include "GLFW/glfw3.h"
#endif




#pragma pack(push, 1)
struct st_BMP_HEADER {
    uint8_t bfType[2]{ 'B', 'M'};
    uint32_t bfSize{ 0 };
    uint32_t bfReserved{ 0 };
    uint32_t bfOffBytes{ 54 };
};

struct st_BMP_INFO_HEADER {
    uint32_t biSize{ 40 };
    int32_t biWidth{ 0 };
    int32_t biHeight{ 0 };
    uint16_t biPlanes{ 1 };
    uint16_t biBitCount{ 24 };
    uint32_t biCompression{ 0 };
    uint32_t biSizeImage{ 0 };
    int32_t biXPixelsPerMeter{ 0 };
    int32_t biYPixelsPerMeter{ 0 };
    uint32_t biClrUsed{ 1 << 24 };
    uint32_t biClrImportant{ 0 };
};
#pragma pack(pop)


template<typename T, uint32_t p>
constexpr T ceilPower2(const T n) {
    // Ceils the number when dividing with 2^p
    // Example: ceil(70 / 32.0) = int(70 / 32) + bool(70 & 31)
    return (n >> p) + bool(p & ((1 << p)-1));
}

glm::fvec3 LinearTosRGB(const glm::fvec3& C) {
    glm::fvec3 sRGB(0.0f);
    sRGB.r = (C.r <= 0.0031308f) ? C.r * 12.92f : 1.055f * powf(C.r, 1.0f/2.4f) - 0.055f;
    sRGB.g = (C.g <= 0.0031308f) ? C.g * 12.92f : 1.055f * powf(C.g, 1.0f/2.4f) - 0.055f;
    sRGB.b = (C.b <= 0.0031308f) ? C.b * 12.92f : 1.055f * powf(C.b, 1.0f/2.4f) - 0.055f;
    return sRGB;
}

glm::fvec3 ACESFilm(const glm::fvec3& x) {
    constexpr float a = 2.51f;
    constexpr float b = 0.03f;
    constexpr float c = 2.43f;
    constexpr float d = 0.59f;
    constexpr float e = 0.14f;
    return glm::clamp((x*(a*x+b))/(x*(c*x+d)+e), glm::fvec3(0.0f), glm::fvec3(1.0f));
}

bool Scene::loadTexture(const std::string &texture_name, uint32_t material_id, uint32_t offset)
{
    int width, height;
    float* image_data = nullptr;
    const char* err = nullptr;

    int ret = LoadEXR(&image_data, &width, &height, texture_name.c_str(), &err);

    if (ret != TINYEXR_SUCCESS) {
        std::cerr << "Couldn't load " << texture_name << std::endl;
        if (err) {
            std::cerr << '\t' << err << std::endl;
            FreeEXRErrorMessage(err);
        }
        return false;
    }

    std::cout << texture_name << '\t' << width << 'x' << height << std::endl;
    glTextureSubImage3D(textureAtlas,
        0,
        0, 0, material_id*3u + offset,
        width, height, 1,
        GL_RGBA, GL_FLOAT, image_data
    );
    free(image_data);

    return true;
}

bool Scene::loadEnvironmentTexture(const std::string &texture_name)
{
    int width, height;
    float* image_data = nullptr;
    const char* err = nullptr;

    int ret = LoadEXR(&image_data, &width, &height, texture_name.c_str(), &err);

    if (ret != TINYEXR_SUCCESS) {
        std::cerr << "Couldn't load " << texture_name << std::endl;
        if (err) {
            std::cerr << '\t' << err << std::endl;
            FreeEXRErrorMessage(err);
        }
        return false;
    }
    std::cout << texture_name << '\t' << width << 'x' << height << std::endl;
    if (environmentTexture)
        glDeleteTextures(1, &environmentTexture);

    glCreateTextures(GL_TEXTURE_2D, 1, &environmentTexture);
    glTextureStorage2D(environmentTexture, 1, GL_RGB32F, width, height);
    glTextureParameteri(environmentTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(environmentTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(environmentTexture, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(environmentTexture, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(environmentTexture, GL_TEXTURE_MAX_LEVEL, 1);
    glTextureSubImage2D(environmentTexture, 0, 0, 0, width, height, GL_RGBA, GL_FLOAT, image_data);

    free(image_data);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, environmentTexture);
    glProgramUniform1i(eyeRayTracerProgram, 2, 2);
    glProgramUniform1i(modelShader.getID(), 2, 2);
    glActiveTexture(GL_TEXTURE0);

    return true;
}


bool Scene::loadMaterial(const std::string &name, uint32_t material_id) {
    if (material_id >= m_materials.size())
        return false;
    
    bool diffuse = loadTexture(name+"_albedo.exr",   material_id, 0);
    bool normal = loadTexture(name+"_normal.exr", material_id, 1);
    bool arm = loadTexture(name+"_arm.exr",    material_id, 2);
    if (diffuse && normal && arm)
    {
        activeTextures[material_id] = 1;
    }
    else {
        activeTextures[material_id] = 0;
    }
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureAtlas);
    glProgramUniform1i(eyeRayTracerProgram, 1, 1);
    glProgramUniform1i(modelShader.getID(), 1, 1);
    glActiveTexture(GL_TEXTURE0);
    glNamedBufferSubData(hasTextureBuffer, 0, sizeof(int)*activeTextures.size(), activeTextures.data());
    return diffuse && normal && arm;
}

Scene::Scene()
    : eyeRayTracerProgram(glCreateProgram()), drawBufferProgram(glCreateProgram()),
      displayShader("./res/shader/displayQuad"), modelShader("./res/shader/model")
{
    GLuint computeID = 0;
    if (loadShaderProgram("./res/shader/raytracer.glsl", GL_COMPUTE_SHADER, computeID))
        glAttachShader(eyeRayTracerProgram, computeID);
    glLinkProgram(eyeRayTracerProgram);
    glDeleteShader(computeID);

    if (loadShaderProgram("./res/shader/createDrawBuffers.glsl", GL_COMPUTE_SHADER, computeID))
        glAttachShader(drawBufferProgram, computeID);
    glLinkProgram(drawBufferProgram);
    glDeleteShader(computeID);

    glCreateVertexArrays(1, &screenVAO);
    glCreateVertexArrays(1, &modelVAO);
    glCreateBuffers(1, &screenBuffer);

    static glm::fvec2 quad[] = {
        glm::fvec2(-1.0f, -1.0f),
        glm::fvec2(1.0f, -1.0f),
        glm::fvec2(-1.0f, 1.0f),
        glm::fvec2(1.0f, 1.0f)
    };

    glNamedBufferStorage(screenBuffer, sizeof(quad), quad, 0);
    glVertexArrayVertexBuffer(screenVAO, 0, screenBuffer, 0, sizeof(glm::fvec2));
    glVertexArrayAttribFormat(screenVAO, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glEnableVertexArrayAttrib(screenVAO, 0);
}

Scene::~Scene() {
    glDeleteVertexArrays(1, &screenVAO);
    glDeleteVertexArrays(1, &modelVAO);
    glDeleteBuffers(1, &screenBuffer);
    glDeleteBuffers(1, &modelBuffer);
    glDeleteBuffers(1, &hasTextureBuffer);
    glDeleteTextures(1, &environmentTexture);
    glDeleteTextures(1, &textureAtlas);
    glDeleteProgram(eyeRayTracerProgram);
}


void Scene::createTrianglesBuffers() {
    computeData.triangles = 0;
    uint32_t light_sources = 0;
    for (const Object &obj: m_objects) {
        computeData.triangles += obj.triangles.size();
        const glm::fvec3 light = m_materials[obj.material_index].emission_ior;
        if (glm::dot(light,light) > 0.0f)
            light_sources += obj.triangles.size();
    }

    glProgramUniform1i(eyeRayTracerProgram, 6, light_sources);

    const uint64_t triangleBytes = computeData.triangles * sizeof(Triangle);

    std::cout << "Triangles: " << computeData.triangles << "\t " << roundf(triangleBytes/1024.0f*100.0f)/100.0f << " KB\n";
    
    Triangle *        const triangles        = new Triangle[computeData.triangles]{};
    TriangleModel *   const triangleModels   = new TriangleModel[computeData.triangles]{};
    TriangleShading * const triangleShadings = new TriangleShading[computeData.triangles]{};

    uint32_t index = light_sources;
    uint32_t light_index = 0;
    
    glm::fvec3 bb_min = m_objects[0].triangles[0].position;
    glm::fvec3 bb_max = m_objects[0].triangles[0].position;

    for (const Object &obj: m_objects) {
        for (const Triangle &tri: obj.triangles) {
            bb_min.x = std::min(tri.position.x, bb_min.x);
            bb_min.y = std::min(tri.position.y, bb_min.y);
            bb_min.z = std::min(tri.position.z, bb_min.z);

            bb_max.x = std::max(tri.position.x, bb_max.x);
            bb_max.y = std::max(tri.position.y, bb_max.y);
            bb_max.z = std::max(tri.position.z, bb_max.z);

            const glm::fvec3 light = m_materials[obj.material_index].emission_ior;
            uint32_t id;
            if (glm::dot(light, light) > 0.0f)
                id = light_index++;
            else
                id = index++;
            triangles[id] = tri;
        }
    }

    const glm::fvec3 bb_center = (bb_min + bb_max) * 0.5f;
    glProgramUniform3f(eyeRayTracerProgram, glGetUniformLocation(eyeRayTracerProgram, "BB_CENTER"), bb_center.x, bb_center.y, bb_center.z);
    glProgramUniform1f(eyeRayTracerProgram, glGetUniformLocation(eyeRayTracerProgram, "EXPOSURE"), 1.0f);
    glProgramUniform1f(modelShader.getID(), 3, 1.0f);

    std::sort(triangles + light_sources, triangles + computeData.triangles, [](const Triangle &a, const Triangle &b) { return glm::length(glm::cross(a.u, a.v)) > glm::length(glm::cross(b.u, b.v)); });

    for (int i=0; i < computeData.triangles; i++) {
        const Triangle &tri = triangles[i];
        new (&triangleModels[i])   TriangleModel  (tri.true_normal, tri.position, tri.u, tri.v);
        new (&triangleShadings[i]) TriangleShading(tri.material_id, tri.normals, tri.tangents, tri.tex_p, tri.tex_u, tri.tex_v);
    }
    delete[] triangles;

    glCreateBuffers(4, computeData.buffer.arr);
    glNamedBufferStorage(computeData.buffer.models,    sizeof(TriangleModel)   * computeData.triangles, triangleModels, 0);
    glNamedBufferStorage(computeData.buffer.shading,   sizeof(TriangleShading) * computeData.triangles, triangleShadings, 0);
    glNamedBufferStorage(computeData.buffer.materials, sizeof(Material)        * computeData.triangles, m_materials.data(), 0);
    delete[] triangleModels, triangleShadings;

    glCreateBuffers(1, &modelBuffer);
    glNamedBufferStorage(modelBuffer, sizeof(Vertex)*3*computeData.triangles, nullptr, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, modelBuffer);

    for (int i=0; i < 7; ++i) {
        glVertexArrayVertexBuffer(modelVAO, i, modelBuffer, 0, sizeof(Vertex));
        glVertexArrayAttribFormat(modelVAO, i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::fvec4)*i);
        glEnableVertexArrayAttrib(modelVAO, i);
    }
}

void Scene::createRTCSData() {
    createTrianglesBuffers();
    glProgramUniform1i(eyeRayTracerProgram, glGetUniformLocation(eyeRayTracerProgram, "COUNT"), computeData.triangles);

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &textureAtlas);
    glTextureStorage3D(textureAtlas, 1, GL_R11F_G11F_B10F, 4096, 4096, m_materials.size() * 3);
    glTextureParameteri(textureAtlas, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(textureAtlas, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(textureAtlas, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(textureAtlas, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(textureAtlas, GL_TEXTURE_MAX_LEVEL, 1);

    activeTextures.resize(m_materials.size());
    std::fill(activeTextures.begin(), activeTextures.end(), 0);

    glCreateBuffers(1, &hasTextureBuffer);
    glNamedBufferStorage(hasTextureBuffer, sizeof(int)*m_materials.size(), activeTextures.data(), GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, hasTextureBuffer);

    std::cout << "Material Count: " << activeTextures.size() << '\n';
}

void Scene::finalizeObjects() {
    if (!computeData.initialized) {
        createRTCSData();
        glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 1, 3, computeData.buffer.arr);

        const uint32_t trisDiv64Ceil = ceilPower2<uint32_t, 6U>(computeData.triangles);

        glUseProgram(drawBufferProgram);
        glDispatchCompute(trisDiv64Ceil, 1, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);

        glFinish();

        computeData.initialized = true;
    }
}

void Scene::adaptResolution(const glm::ivec2 &newRes) {
    const unsigned long newSize = newRes.x * newRes.y;
    const unsigned long oldSize = computeData.resolution.x * computeData.resolution.y;
    computeData.resolution = newRes;

    if (newSize != oldSize) {
        glDeleteTextures(1, &computeData.renderTarget);
        glDeleteTextures(1, &computeData.renderTargetLow);
        glCreateTextures(GL_TEXTURE_2D, 1, &computeData.renderTarget);
        glTextureStorage2D(computeData.renderTarget, 1, GL_RGBA32F, newRes.x, newRes.y);
        glTextureParameteri(computeData.renderTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(computeData.renderTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(computeData.renderTarget, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTextureParameteri(computeData.renderTarget, GL_TEXTURE_WRAP_T, GL_CLAMP);

        glCreateTextures(GL_TEXTURE_2D, 1, &computeData.renderTargetLow);
        glTextureStorage2D(computeData.renderTargetLow, 1, GL_RGBA32F, (int)(180.0/newRes.y*newRes.x), 180); // >> 3
        glTextureParameteri(computeData.renderTargetLow, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(computeData.renderTargetLow, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        if (newSize > computeData.buffer_res.x*computeData.buffer_res.y) {
            computeData.buffer_res = newRes;
        }
    }
}

void Scene::traceScene(const uint32_t width, const uint32_t height, const uint32_t sample) {
    const uint32_t widthDivCeil  = ceilPower2<uint32_t, 3U>(width);
    const uint32_t heightDivCeil = ceilPower2<uint32_t, 3U>(height);

    static const int SAMPLEloc = glGetUniformLocation(eyeRayTracerProgram, "SAMPLE");
    glProgramUniform1ui(eyeRayTracerProgram, SAMPLEloc, sample);

    glUseProgram(eyeRayTracerProgram);
    glDispatchCompute(widthDivCeil, heightDivCeil, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void Scene::prepare(int &width, int &height, bool moving, const glm::fmat4 &Camera) {
    static bool firstTime = true;
    if (firstTime) {
        adaptResolution({ width, height });
        firstTime = false;
    }
    
    static const int CAMERAloc = glGetUniformLocation(eyeRayTracerProgram, "CAMERA");
    static const int RECloc = glGetUniformLocation(eyeRayTracerProgram, "RECURSION");
    static const int CLOCKloc = glGetUniformLocation(eyeRayTracerProgram, "CLOCK");

    glProgramUniformMatrix4fv(eyeRayTracerProgram, CAMERAloc, 1, GL_FALSE, &Camera[0].x);
    glProgramUniform1ui(eyeRayTracerProgram, CLOCKloc, 95834783); // clock() 95834783

    if (moving) {
        glBindImageTexture(0, computeData.renderTargetLow, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glBindTextureUnit(0, computeData.renderTargetLow);

        width = (int)(240.0/height*width);
        height = 240;

        glProgramUniform1i(eyeRayTracerProgram, RECloc, 2);
    }
    else {
        glBindImageTexture(0, computeData.renderTarget, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glBindTextureUnit(0, computeData.renderTarget);

        glProgramUniform1i(eyeRayTracerProgram, RECloc, 6);
    }
}

void Scene::display(unsigned int sample) {
    displayShader.Bind();
    displayShader.setUInt("SAMPLES", sample+1);

    glBindVertexArray(screenVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void Scene::renderWireframe(const glm::fmat4 &MVP, const glm::fvec3 &cam_pos) {
    modelShader.setBool("WIREFRAME", true);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    forwardRender(MVP, cam_pos);

    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    modelShader.setBool("WIREFRAME", false);
}

void Scene::forwardRender(const glm::fmat4 &MVP, const glm::fvec3 &cam_pos) {
    modelShader.Bind();
    modelShader.setMatrixFloat4("MVP", MVP);
    modelShader.setFloat3("CAMERA", cam_pos);
    glBindVertexArray(modelVAO);
    glDrawArrays(GL_TRIANGLES, 0, computeData.triangles*3);
}


void Scene::exportEXR(const char *name) const {
    unsigned long pixel_count = (unsigned long)computeData.resolution.x*computeData.resolution.y;
    std::unique_ptr<glm::fvec4[]> raw_pixels(new glm::fvec4[pixel_count]);
    glGetTextureImage(computeData.renderTarget, 0, GL_RGB, GL_FLOAT,
                      pixel_count * sizeof(glm::fvec4),
                      &raw_pixels[0]);
    
    const char * error = nullptr;
    const int ret = SaveEXR(&raw_pixels[0].r, computeData.resolution.x, computeData.resolution.y, 3, false, name, &error);
    if (ret != TINYEXR_SUCCESS) {
        fprintf(stderr, "Save EXR err: %s\n", error);
        FreeEXRErrorMessage(error);
    }
}

void Scene::exportRAW(const char *name) const {
    unsigned long pixel_count = (unsigned long)computeData.resolution.x*computeData.resolution.y;
    std::unique_ptr<glm::fvec4[]> raw_pixels(new glm::fvec4[pixel_count]);
    glGetTextureImage(computeData.renderTarget, 0, GL_RGBA, GL_FLOAT,
                      pixel_count * sizeof(glm::fvec4),
                      &raw_pixels[0]);
    std::ofstream f(name, std::ios::binary);
    f.write(reinterpret_cast<char*>(raw_pixels.get()), pixel_count * sizeof(glm::fvec4));
    f.close();
}

std::shared_ptr<unsigned char[]> Scene::exportRGBA8() const  {
    unsigned long pixel_count = (unsigned long)computeData.resolution.x*computeData.resolution.y;

    std::shared_ptr<unsigned char[]> bmpData(new unsigned char[pixel_count*4]);

    std::unique_ptr<glm::fvec4[]> raw_pixels(new glm::fvec4[pixel_count]);
    glGetTextureImage(computeData.renderTarget, 0, GL_RGBA, GL_FLOAT,
                      pixel_count * sizeof(glm::fvec4),
                      raw_pixels.get());

    for (int i = 0; i < pixel_count; ++i) {
        const glm::fvec4 &pixel = raw_pixels[i];
        bmpData[i * 4 + 0] = static_cast<unsigned char>(pixel.r * 255);
        bmpData[i * 4 + 1] = static_cast<unsigned char>(pixel.g * 255); // glm::pow(pixel.g, 1.6f)
        bmpData[i * 4 + 2] = static_cast<unsigned char>(pixel.b * 255);
        bmpData[i * 4 + 3] = static_cast<unsigned char>(255);
    }

    return bmpData;
}

bool Scene::exportBMP(const char *name) const
{
    unsigned long pixel_count = (unsigned long)computeData.resolution.x*computeData.resolution.y;

    std::unique_ptr<glm::fvec4[]> raw_pixels(new glm::fvec4[pixel_count]);
    glGetTextureImage(computeData.renderTarget, 0, GL_RGBA, GL_FLOAT,
                      (unsigned long)(pixel_count * sizeof(glm::fvec4)),
                      &raw_pixels[0]);


    st_BMP_HEADER header;
    st_BMP_INFO_HEADER info;
    info.biWidth = computeData.resolution.x;
    info.biHeight = computeData.resolution.y;

    std::ofstream bmpFile(name, std::ios::binary);
    bmpFile.write(reinterpret_cast<char*>(&header), sizeof(header));
    bmpFile.write(reinterpret_cast<char*>(&info), sizeof(info));

    for (unsigned long i = 0; i < pixel_count; ++i)
    {
        const glm::fvec3 mappedColor = LinearTosRGB(ACESFilm(raw_pixels[i]));
        bmpFile << static_cast<unsigned char>(glm::clamp(mappedColor.b * 256.0f, 0.0f, 255.0f));
        bmpFile << static_cast<unsigned char>(glm::clamp(mappedColor.g * 256.0f, 0.0f, 255.0f));
        bmpFile << static_cast<unsigned char>(glm::clamp(mappedColor.r * 256.0f, 0.0f, 255.0f));
    }

    bmpFile.close();

    return true;
}


struct st_wf_face {
    st_wf_face() = default;

    unsigned int pos_i[3];
    unsigned int tex_i[3];
    unsigned int nrm_i[3];
};


bool Scene::addWavefrontModel(const std::string &name) {
    std::ifstream objFile(name + ".obj");
    if (!objFile || !readWFMaterial(name))
        return false;

    Object *object = nullptr;
    std::vector<glm::fvec3> positions;
    std::vector<glm::fvec3> normals;
    std::vector<glm::fvec2> uv_coords;
    std::vector<st_wf_face> faces;

    std::string line;

    while (getline(objFile, line)) {
        if (line.length() <= 2)
            continue;

        if (line[0] == 'o') {
            if (object) {
                for (const st_wf_face &face: faces) {
                    object->triangles.emplace_back(
                        positions[face.pos_i[0] - 1], positions[face.pos_i[1] - 1], positions[face.pos_i[2] - 1],
                        normals[face.nrm_i[0] - 1], normals[face.nrm_i[1] - 1], normals[face.nrm_i[2] - 1],
                        uv_coords[face.tex_i[0] - 1], uv_coords[face.tex_i[1] - 1], uv_coords[face.tex_i[2] - 1],
                        object->material_index
                    );
                }
            }
            faces.clear();
            object = &getObject(std::string(line.begin() + 2, line.end()));
        } else if (line[0] == 'v') {
            switch (line[1]) {
                case ' ': {
                    glm::fvec3 &p = positions.emplace_back();
                    SSCANF(line.c_str(), "%*s %f %f %f", &p.x, &p.y, &p.z);
                }
                    break;
                case 't': {
                    glm::fvec2 &uv = uv_coords.emplace_back();
                    SSCANF(line.c_str(), "%*s %f %f", &uv.x, &uv.y);
                }
                    break;
                case 'n': {
                    glm::fvec3 &n = normals.emplace_back();
                    SSCANF(line.c_str(), "%*s %f %f %f", &n.x, &n.y, &n.z);
                }
                    break;
            }
        } else if (object != nullptr && line.compare(0, 6, "usemtl") == 0) {
            object->material_index = getMaterialIndex(line.substr(7));
        } else if (line[0] == 'f') {
            st_wf_face &face = faces.emplace_back();
            SSCANF(line.c_str(), "%*s %i/%i/%i %i/%i/%i %i/%i/%i",
                   &face.pos_i[0], &face.tex_i[0], &face.nrm_i[0],
                   &face.pos_i[1], &face.tex_i[1], &face.nrm_i[1],
                   &face.pos_i[2], &face.tex_i[2], &face.nrm_i[2]
            );
        }
    }
    if (object) {
        for (const st_wf_face &face: faces) {
            object->triangles.emplace_back(
                positions[face.pos_i[0] - 1], positions[face.pos_i[1] - 1], positions[face.pos_i[2] - 1],
                normals[face.nrm_i[0] - 1], normals[face.nrm_i[1] - 1], normals[face.nrm_i[2] - 1],
                uv_coords[face.tex_i[0] - 1], uv_coords[face.tex_i[1] - 1], uv_coords[face.tex_i[2] - 1],
                object->material_index
            );
        }
    }
    objFile.close();

    return true;
}

bool Scene::readWFMaterial(const std::string &material_name) {
    std::ifstream mtlFile(material_name + ".mtl");
    if (!mtlFile)
        return false;

    Material *material_ptr = nullptr;
    std::string line;

    while (getline(mtlFile, line)) {
        if (line.length() <= 3)
            continue;

        if (line.compare(0, 6, "newmtl") == 0) {
            material_ptr = &getMaterial(line.substr(7).c_str());
        }

        if (material_ptr) {
            Material &material = *material_ptr;
            if (line.compare(0, 2, "Kd") == 0)
                SSCANF(line.c_str(), "%*s %f %f %f", &material.albedo.r, &material.albedo.g, &material.albedo.b);

            else if (line.compare(0, 2, "Ks") == 0)
                SSCANF(line.c_str(), "%*s %f %f %f", &material.specular_roughness.r, &material.specular_roughness.g, &material.specular_roughness.b);

            else if (line.compare(0, 2, "Ke") == 0)
                SSCANF(line.c_str(), "%*s %f %f %f", &material.emission_ior.r, &material.emission_ior.g, &material.emission_ior.b);

            else if (line.compare(0, 2, "Ni") == 0) {
                SSCANF(line.c_str(), "%*s %f", &material.emission_ior.a);
            }
            else if (line.compare(0, 2, "Ns") == 0) {
                SSCANF(line.c_str(), "%*s %f", &material.specular_roughness.a);
                material.specular_roughness.a /= 1000.0f;
            }
        }
    }

    mtlFile.close();

    for (const auto &mat : m_material_indices) {
        std::cout << mat.first << "\t:\t" << mat.second << '\n';
    }
    return true;
}
