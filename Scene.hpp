#include <string>
#include <vector>
#include <unordered_map>
#include "Material.hpp"
#include "3Dobjects.hpp"
#include "RTCSBuffer.hpp"
#include "Shader.hpp"
#include <memory>


class Scene {
public:
    Scene();
    ~Scene();

    bool addWavefrontModel(const std::string &model_name);

    void setDirectionLight(const glm::fvec3 &light_dir);
    void setAmbientLight(const glm::fvec3 &ambient_color);

    void finalizeObjects();
    void generateRandomUnitVectors();
    void adaptResolution(const glm::ivec2 &newRes);
    void render(int width, int height, bool moving, const glm::fmat4 &Camera, unsigned int sample);
    void display(unsigned int sample);
    void renderWireframe(const glm::fmat4 &MVP, const glm::fvec3 &cam_pos);
    void forwardRender(const glm::fmat4 &MVP, const glm::fvec3 &cam_pos);
    std::shared_ptr<unsigned char[]> exportRGBA8() const;
    bool exportBMP(const char *name) const;
    void exportRAW(const char *name) const;
    void traceScene(int width, int height, const glm::fmat4 &MVP, int recursion, unsigned int sample);

    inline Object &getObject(std::string &&name) {
        return m_objects.emplace_back(std::move(name));
    }

    inline int getMaterialIndex(const std::string &name) const {
        if (!m_material_indices.contains(name))
            return -1;

        return m_material_indices.at(name);
    }

    inline Material &getMaterial(const std::string &name) {
        if (m_material_indices.contains(name))
            return m_materials[m_material_indices.at(name)];

        m_material_indices[name] = (int)m_materials.size();
        return m_materials.emplace_back();
    }

private:
    st_RTCS_data computeData;

    GLuint rayBuffer{ 0 };
    GLuint randomBuffer{ 0 };

    GLuint modelBuffer{ 0 };
    GLuint modelVAO;

    GLuint screenBuffer;
    GLuint screenVAO;

    Shader displayShader;
    Shader modelShader;

    GLuint eyeRayTracerProgram;
    GLuint drawBufferProgram;

    void createTrianglesBuffers();

    void createMaterialsBuffers();

    void createRTCSData();

    void bindBuffer();

    bool readWFMaterial(const std::string &material_name);

    std::vector<Object> m_objects;
    std::vector<Material> m_materials;
    std::unordered_map<std::string, int> m_material_indices;
};



