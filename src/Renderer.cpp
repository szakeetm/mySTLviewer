#include "Renderer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>

Renderer::Renderer()
    : m_VAO(0), m_VBO(0), m_EBO(0), m_shaderProgram(0), m_renderMode(RenderMode::WIREFRAME) {
}

Renderer::~Renderer() {
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_EBO) glDeleteBuffers(1, &m_EBO);
    if (m_shaderProgram) glDeleteProgram(m_shaderProgram);
}

bool Renderer::initialize() {
    if (!loadShaders()) {
        std::cerr << "Failed to load shaders" << std::endl;
        return false;
    }
    return true;
}

void Renderer::setMesh(std::unique_ptr<Mesh> mesh) {
    m_mesh = std::move(mesh);
    setupMesh();
}

void Renderer::setupMesh() {
    if (!m_mesh || m_mesh->vertices.empty()) {
        return;
    }
    
    // Delete old buffers if they exist
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_EBO) glDeleteBuffers(1, &m_EBO);
    
    // Generate buffers
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);
    
    glBindVertexArray(m_VAO);
    
    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_mesh->vertices.size() * sizeof(Vertex),
                 m_mesh->vertices.data(), GL_STATIC_DRAW);
    
    // Upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_mesh->indices.size() * sizeof(unsigned int),
                 m_mesh->indices.data(), GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    
    std::cout << "Mesh setup complete: " << m_mesh->vertices.size() 
              << " vertices, " << m_mesh->indices.size() / 3 << " triangles" << std::endl;
}

void Renderer::render(const glm::mat4& projection, const glm::mat4& view, const glm::mat4& model) {
    if (!m_mesh || m_mesh->vertices.empty() || !m_shaderProgram) {
        return;
    }
    
    glUseProgram(m_shaderProgram);
    
    // Set uniforms
    GLint projLoc = glGetUniformLocation(m_shaderProgram, "projection");
    GLint viewLoc = glGetUniformLocation(m_shaderProgram, "view");
    GLint modelLoc = glGetUniformLocation(m_shaderProgram, "model");
    GLint modeLoc = glGetUniformLocation(m_shaderProgram, "renderMode");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniform1i(modeLoc, m_renderMode == RenderMode::WIREFRAME ? 1 : 0);
    
    // Set render mode
    if (m_renderMode == RenderMode::WIREFRAME) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(1.5f);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_LINE_SMOOTH);
    }
    
    // Draw
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, m_mesh->indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Reset polygon mode
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void Renderer::setRenderMode(RenderMode mode) {
    m_renderMode = mode;
}

bool Renderer::loadShaders() {
    // Read shader files
    std::ifstream vShaderFile("shaders/vertex.glsl");
    std::ifstream fShaderFile("shaders/fragment.glsl");
    
    if (!vShaderFile.is_open() || !fShaderFile.is_open()) {
        std::cerr << "Failed to open shader files" << std::endl;
        return false;
    }
    
    std::stringstream vShaderStream, fShaderStream;
    vShaderStream << vShaderFile.rdbuf();
    fShaderStream << fShaderFile.rdbuf();
    
    std::string vertexCode = vShaderStream.str();
    std::string fragmentCode = fShaderStream.str();
    
    // Compile shaders
    GLuint vertexShader = compileShader(vertexCode, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader(fragmentCode, GL_FRAGMENT_SHADER);
    
    if (!vertexShader || !fragmentShader) {
        return false;
    }
    
    // Link program
    m_shaderProgram = linkProgram(vertexShader, fragmentShader);
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return m_shaderProgram != 0;
}

GLuint Renderer::compileShader(const std::string& source, GLenum type) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed:\n" << infoLog << std::endl;
        return 0;
    }
    
    return shader;
}

GLuint Renderer::linkProgram(GLuint vertexShader, GLuint fragmentShader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Program linking failed:\n" << infoLog << std::endl;
        return 0;
    }
    
    return program;
}
