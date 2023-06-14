#include <iostream>
#include <fstream>
#include "Scene.hpp"
#include "Shader.hpp"
#include <random>


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




// Automatically maps and unmaps the Buffer
template<typename T>
struct MappedBuffer {
    MappedBuffer(GLuint &buffer_id, unsigned int count,
                 GLbitfield buffer_flags = GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT, GLenum access = GL_WRITE_ONLY)
                 : buffer(buffer_id), ptr(nullptr) {
        if (buffer_id == 0 && count > 0) {
            glCreateBuffers(1, &buffer_id);
            glNamedBufferStorage(buffer_id, count * sizeof(T), nullptr, buffer_flags);
        }
        ptr = (T *) glMapNamedBuffer(buffer_id, access);
    }

    ~MappedBuffer() { glUnmapNamedBuffer(buffer); }

    inline T &operator[](unsigned int index) { return ptr[index]; }

    GLuint buffer;
    T *ptr;
};


Scene::Scene()
    : eyeRayTracerProgram(glCreateProgram()), spatialSortProgram(glCreateProgram()), drawBufferProgram(glCreateProgram()),
      displayShader("./res/shader/displayQuad"), modelShader("./res/shader/model")
{
    GLuint computeID = 0;
    if (loadShaderProgram("./res/shader/raytracer.glsl", GL_COMPUTE_SHADER, computeID))
        glAttachShader(eyeRayTracerProgram, computeID);
    glLinkProgram(eyeRayTracerProgram);
    glDeleteShader(computeID);

    if (loadShaderProgram("./res/shader/spatialSort.glsl", GL_COMPUTE_SHADER, computeID))
        glAttachShader(spatialSortProgram, computeID);
    glLinkProgram(spatialSortProgram);
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
    glDeleteProgram(eyeRayTracerProgram);
    glDeleteProgram(spatialSortProgram);
}


void Scene::createTrianglesBuffers() {
    computeData.triangles = 0;
    for (const Object &obj: m_objects)
        computeData.triangles += obj.triangles.size();
    std::cout << "Triangles: " << computeData.triangles << '\n';

    MappedBuffer<Triangle> mapped_triangles(computeData.triangleBuffer, computeData.triangles);

    unsigned int index = 0;
    for (Object &obj: m_objects) {
        for (Triangle &tri: obj.triangles) {
            mapped_triangles[index++] = tri;
        }
    }

    glCreateBuffers(1, &computeData.visibility);
    glNamedBufferStorage(computeData.visibility, (GLsizeiptr)computeData.triangles*computeData.triangles*sizeof(int), nullptr, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, computeData.visibility);

    glCreateBuffers(1, &modelBuffer);
    glNamedBufferStorage(modelBuffer, (long)computeData.triangles*3*sizeof(Vertex), nullptr, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, modelBuffer);

    for (int i=0; i < 5; ++i) {
        glVertexArrayVertexBuffer(modelVAO, i, modelBuffer, sizeof(glm::fvec4)*i, sizeof(Vertex));
        glVertexArrayAttribFormat(modelVAO, i, 4, GL_FLOAT, GL_FALSE, 0);
        glEnableVertexArrayAttrib(modelVAO, i);
    }
}

void Scene::createMaterialsBuffers() {
    unsigned int material_count = m_materials.size();

    MappedBuffer<Material> mapped_material(computeData.materialBuffer, material_count);

    for (unsigned int index = 0; index < material_count; ++index) {
        mapped_material[index] = m_materials[index];
    }
}

void Scene::createRTCSData() {
    createTrianglesBuffers();
    createMaterialsBuffers();
    glProgramUniform1i(eyeRayTracerProgram, glGetUniformLocation(eyeRayTracerProgram, "COUNT"), computeData.triangles);
    glProgramUniform1i(spatialSortProgram, glGetUniformLocation(spatialSortProgram, "COUNT"), computeData.triangles);
}


void Scene::bindBuffer() {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, computeData.triangleBuffer);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, computeData.materialBuffer);
}

void Scene::finalizeObjects() {
    if (!computeData.initialized) {
        createRTCSData();
        bindBuffer();

        double t0 = glfwGetTime();
        glUseProgram(spatialSortProgram);
        glDispatchCompute((int)glm::ceil(computeData.triangles / 64.0f), 1, 1);

        glUseProgram(drawBufferProgram);
        glDispatchCompute((int)glm::ceil(computeData.triangles / 64.0f), 1, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        std::cout << glfwGetTime() - t0 << std::endl;

        computeData.initialized = true;
    }
}

void Scene::generateRandomUnitVectors() {
    static std::default_random_engine generator;
    static std::uniform_real_distribution<double> angleDist(0.0, std::numbers::pi);
    long new_random_size = computeData.resolution.x*computeData.resolution.y*(long)4;

    static MappedBuffer<glm::fvec4> rndMap(randomBuffer, 0, GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT, GL_WRITE_ONLY);
    static long random_size = 0;

    if (new_random_size > random_size) {
        glDeleteBuffers(1, &randomBuffer);
        glCreateBuffers(1, &randomBuffer);
        glNamedBufferStorage(randomBuffer, new_random_size * sizeof(glm::fvec4), nullptr, GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);
        rndMap.ptr = (glm::fvec4*)glMapNamedBuffer(randomBuffer, GL_WRITE_ONLY);
        rndMap.buffer = randomBuffer;
        random_size = new_random_size;
    }
    if (!rndMap.ptr)
        return;

    unsigned index = 0;
    for (int x=0; x < computeData.resolution.x; ++x) {
        for (int y=0; y < computeData.resolution.y; ++y) {
            for (int z=0; z < 4; ++z) {
                double wx(angleDist(generator)), wy(angleDist(generator)*2.0);
                rndMap[index++] = {
                        glm::cos(wx)*glm::sin(wy),
                        -glm::sin(wx),
                        glm::cos(wx)*glm::cos(wy),
                        0.0
                };
            }
        }
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, randomBuffer);
}

void Scene::setDirectionLight(const glm::fvec3 &light_dir) {
    static const int RT_LightLoc = glGetUniformLocation(eyeRayTracerProgram, "LIGHT_DIR");
    glProgramUniform3f(eyeRayTracerProgram, RT_LightLoc, light_dir.x, light_dir.y, light_dir.z);
    modelShader.setFloat3("LIGHT_DIR", light_dir);
}

void Scene::setAmbientLight(const glm::fvec3 &ambient_color) {
    static const int RT_AmbientLoc = glGetUniformLocation(eyeRayTracerProgram, "AMBIENT");
    glProgramUniform3f(eyeRayTracerProgram, RT_AmbientLoc, ambient_color.r, ambient_color.g, ambient_color.b);
    modelShader.setFloat3("AMBIENT", ambient_color);
}

void Scene::adaptResolution(const glm::ivec2 &newRes) {
    unsigned long newSize = newRes.x * newRes.y;
    unsigned long oldSize = computeData.resolution.x * computeData.resolution.y;
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
            glDeleteBuffers(1, &rayBuffer);
            glCreateBuffers(1, &rayBuffer);
            glNamedBufferStorage(rayBuffer, (GLsizeiptr)newRes.x * newRes.y * sizeof(glm::fvec4), nullptr, 0);
            computeData.buffer_res = newRes;
        }
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, rayBuffer);

    static bool hasRand = false;
    if (hasRand)
        return;

    hasRand = true;
    generateRandomUnitVectors();
}

void Scene::traceScene(int width, int height, const glm::fmat4 &Camera, int recursion, unsigned int sample) {
    static const int CAMERAloc = glGetUniformLocation(eyeRayTracerProgram, "CAMERA");
    static const int RECloc = glGetUniformLocation(eyeRayTracerProgram, "RECURSION");
    static const int SAMPLEloc = glGetUniformLocation(eyeRayTracerProgram, "SAMPLE");

    glProgramUniformMatrix4fv(eyeRayTracerProgram, CAMERAloc, 1, GL_FALSE, &Camera[0].x);
    glProgramUniform1i(eyeRayTracerProgram, RECloc, recursion);
    glProgramUniform1i(eyeRayTracerProgram, SAMPLEloc, sample);

    glUseProgram(eyeRayTracerProgram);
    glDispatchCompute(((int)glm::ceil((float)width / 8.0f)), ((int)glm::ceil((float)height / 8.0f)), 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void Scene::render(int width, int height, bool moving, const glm::fmat4 &Camera, unsigned int sample) {
    static bool firstTime = true;
    if (firstTime) {
        adaptResolution({ width, height });
        firstTime = false;
    }

    int recursion = 12;

    if (moving) {
        glBindImageTexture(0, computeData.renderTargetLow, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glBindTextureUnit(0, computeData.renderTargetLow);

        width = (int)(240.0/height*width);
        height = 240;

        recursion = 4;
    }
    else {
        glBindImageTexture(0, computeData.renderTarget, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glBindTextureUnit(0, computeData.renderTarget);
    }

    if (!moving && sample > 0 && sample % 4 == 0)
        generateRandomUnitVectors();

    traceScene(width, height, Camera, recursion, sample);

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


void Scene::exportRAW(const char *name) const {
    unsigned long pixel_count = (unsigned long)computeData.resolution.x*computeData.resolution.y;
    std::unique_ptr<glm::fvec4[]> raw_pixels(new glm::fvec4[pixel_count]);
    glGetTextureImage(computeData.renderTarget, 0, GL_RGBA, GL_FLOAT,
                      pixel_count * sizeof(glm::fvec4),
                      &raw_pixels[0]);
    std::ofstream f(name, std::ios::binary);
    f.write(reinterpret_cast<char*>(&raw_pixels[0]), pixel_count * sizeof(glm::fvec4));
    f.close();
}

std::shared_ptr<unsigned char[]> Scene::exportRGBA8() const  {
    unsigned long pixel_count = (unsigned long)computeData.resolution.x*computeData.resolution.y;

    std::shared_ptr<unsigned char[]> bmpData(new unsigned char[pixel_count*4]);

    auto *raw_pixels = new glm::fvec4[pixel_count];
    glGetTextureImage(computeData.renderTarget, 0, GL_RGBA, GL_FLOAT,
                      pixel_count * sizeof(glm::fvec4),
                      raw_pixels);

    for (int i = 0; i < pixel_count; ++i) {
        const glm::fvec4 &pixel = raw_pixels[i];
        bmpData[i * 4 + 0] = static_cast<unsigned char>(pixel.r * 255);
        bmpData[i * 4 + 1] = static_cast<unsigned char>(pixel.g * 255); // glm::pow(pixel.g, 1.6f)
        bmpData[i * 4 + 2] = static_cast<unsigned char>(pixel.b * 255);
        bmpData[i * 4 + 3] = static_cast<unsigned char>(255);
    }

    delete[] raw_pixels;

    return bmpData;
}

bool Scene::exportBMP(const char *name) const {
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

    for (unsigned long i = 0; i < pixel_count; ++i) {
        bmpFile << static_cast<unsigned char>(glm::clamp(raw_pixels[i].b * 255, 0.0f, 255.0f));
        bmpFile << static_cast<unsigned char>(glm::clamp(raw_pixels[i].g * 255, 0.0f, 255.0f));
        bmpFile << static_cast<unsigned char>(glm::clamp(raw_pixels[i].b * 255, 0.0f, 255.0f));
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
            }
        }
    }

    mtlFile.close();
    return true;
}
