#include "Shader.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>


bool loadShaderProgram(const std::string&& filename, GLint shaderType, GLuint &shaderID) {
    std::string sourceCode;
    std::ifstream shaderFile(filename);

    if (shaderFile) {
        std::stringstream sourceStringStream;
        sourceStringStream << shaderFile.rdbuf();

        shaderFile.close();

        sourceCode = sourceStringStream.str();
    } else {
        return false;
    }

    int success;
    char infoLog[1024];

    const char *source_cptr = sourceCode.c_str();

    shaderID = glCreateShader(shaderType);
    glShaderSource(shaderID, 1, &source_cptr, nullptr);
    glCompileShader(shaderID);

    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);
    std::cout << filename.substr(filename.size()-2, 2) << ' ';
    if (!success) {
        glGetShaderInfoLog(shaderID, 1024, nullptr, infoLog);
        std::cout << "[ ERROR  ][Shader ] Error in: " << filename << std::endl << infoLog << std::endl;
        return false;
    }

    return true;
}




Shader::Shader(std::string_view shaderPath)
        : m_shaderName(shaderPath)
{
    Load();
}

bool Shader::Reload() noexcept {
    if (m_programID != 0) {
        glUseProgram(0);
        glDeleteProgram(m_programID);
    }

    m_uniforms.clear();
    return Load();
}

bool Shader::Load() noexcept {
    std::cout << "[  INFO  ][Shader ] Create Shader: " << m_shaderName << " - ";

    GLuint vertexID(0), fragmentID(0), geometryID(0), computeID(0);
    bool hasVertex   = loadShaderProgram(m_shaderName + ".vs", GL_VERTEX_SHADER, vertexID);
    bool hasFragment = loadShaderProgram(m_shaderName + ".fs", GL_FRAGMENT_SHADER, fragmentID);
    bool hasGeometry = loadShaderProgram(m_shaderName + ".gs", GL_GEOMETRY_SHADER, geometryID);
    bool hasCompute = loadShaderProgram(m_shaderName + ".cs", GL_COMPUTE_SHADER, computeID);

    std::cout << '\n';

    // no shader source found
    if (!hasVertex && !hasFragment && !hasGeometry && !hasCompute)
        return false;

    // Create and Link the Shader Program, attach and delete available Shaders
    m_programID = glCreateProgram();

    if (hasVertex)
        glAttachShader(m_programID, vertexID);
    if (hasFragment)
        glAttachShader(m_programID, fragmentID);
    if (hasGeometry)
        glAttachShader(m_programID, geometryID);
    if (hasCompute)
        glAttachShader(m_programID, computeID);

    glLinkProgram(m_programID);

    if (hasVertex)
        glDeleteShader(vertexID);
    if (hasFragment)
        glDeleteShader(fragmentID);
    if (hasGeometry)
        glDeleteShader(geometryID);
    if (hasCompute)
        glDeleteShader(computeID);

    int success;
    char infoLog[1024];
    glGetProgramiv(m_programID, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(m_programID, 1024, nullptr, infoLog);
        std::cout << "[ ERROR  ][Shader ] Link:" << std::endl << infoLog << std::endl;
        return false;
    }

    return true;
}

void Shader::setBool(std::string_view name, bool value) noexcept {
    if (m_uniforms.find(name) == m_uniforms.end())
        m_uniforms[name] = glGetUniformLocation(m_programID, name.data());

    glProgramUniform1i(m_programID, m_uniforms[name], (int) value);
}

void Shader::setInt(std::string_view name, int value) noexcept {
    if (m_uniforms.find(name) == m_uniforms.end())
        m_uniforms[name] = glGetUniformLocation(m_programID, name.data());

    glProgramUniform1i(m_programID, m_uniforms[name], value);
}

void Shader::setUInt(std::string_view name, unsigned int value) noexcept {
    if (m_uniforms.find(name) == m_uniforms.end())
        m_uniforms[name] = glGetUniformLocation(m_programID, name.data());

    glProgramUniform1ui(m_programID, m_uniforms[name], value);
}

void Shader::setFloat(std::string_view name, float value) noexcept {
    if (m_uniforms.find(name) == m_uniforms.end())
        m_uniforms[name] = glGetUniformLocation(m_programID, name.data());

    glProgramUniform1f(m_programID, m_uniforms[name], value);
}

void Shader::setDouble(std::string_view name, double value) noexcept {
    if (m_uniforms.find(name) == m_uniforms.end())
        m_uniforms[name] = glGetUniformLocation(m_programID, name.data());

    glProgramUniform1d(m_programID, m_uniforms[name], value);
}

void Shader::setFloat2(std::string_view name, const glm::fvec2 &value) noexcept {
    if (m_uniforms.find(name) == m_uniforms.end())
        m_uniforms[name] = glGetUniformLocation(m_programID, name.data());

    glProgramUniform2fv(m_programID, m_uniforms[name], 1, &value.x);
}

void Shader::setFloat3(std::string_view name, const glm::fvec3 &value) noexcept {
    if (m_uniforms.find(name) == m_uniforms.end())
        m_uniforms[name] = glGetUniformLocation(m_programID, name.data());

    glProgramUniform3fv(m_programID, m_uniforms[name], 1, &value.x);
}

void Shader::setDouble3(std::string_view name, const glm::dvec3 &value) noexcept {
    if (m_uniforms.find(name) == m_uniforms.end())
        m_uniforms[name] = glGetUniformLocation(m_programID, name.data());

    glProgramUniform3dv(m_programID, m_uniforms[name], 1, &value.x);
}

void Shader::setMatrixFloat4(std::string_view name, const glm::fmat4 &matrix) noexcept {
    if (m_uniforms.find(name) == m_uniforms.end())
        m_uniforms[name] = glGetUniformLocation(m_programID, name.data());

    glProgramUniformMatrix4fv(m_programID, m_uniforms[name], 1, GL_FALSE, &matrix[0].x);
}

Shader::~Shader() {
    std::cout << "[  INFO  ][Shader ] Delete Shader: " << m_shaderName << '(' << m_programID << ')' << std::endl;

    glUseProgram(0);
    glDeleteProgram(m_programID);
    m_uniforms.clear();
}