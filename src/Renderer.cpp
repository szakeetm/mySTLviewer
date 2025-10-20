#include "Renderer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>

Renderer::Renderer()
    : m_VAO(0), m_VBO(0), m_EBO(0), m_shaderProgramSolid(0), m_shaderProgramWireframe(0), m_renderMode(RenderMode::WIREFRAME) {
}

Renderer::~Renderer() {
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_EBO) glDeleteBuffers(1, &m_EBO);
    if (m_shaderProgramSolid) glDeleteProgram(m_shaderProgramSolid);
    if (m_shaderProgramWireframe) glDeleteProgram(m_shaderProgramWireframe);
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
    if (!m_mesh || m_mesh->vertices.empty()) {
        return;
    }
    
    // Choose shader program based on mode
    GLuint program = (m_renderMode == RenderMode::WIREFRAME) ? m_shaderProgramWireframe : m_shaderProgramSolid;
    if (!program) return;
    glUseProgram(program);
    
    // Set uniforms
    GLint projLoc = glGetUniformLocation(program, "projection");
    GLint viewLoc = glGetUniformLocation(program, "view");
    GLint modelLoc = glGetUniformLocation(program, "model");
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    
    // Set render mode and related state
    if (m_renderMode == RenderMode::WIREFRAME) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        // Rely on MSAA for anti-aliasing; GL_LINE_SMOOTH can cause dimming on macOS
        glDisable(GL_LINE_SMOOTH);
        glLineWidth(1.5f);
        // Use opaque lines; blending can dim appearance on macOS
        glDisable(GL_BLEND);
        // Show all edges
        glDisable(GL_CULL_FACE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_LINE_SMOOTH);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
    
    // Draw
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, m_mesh->indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Reset polygon mode (keep other state as set for the next frame)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void Renderer::setRenderMode(RenderMode mode) {
    m_renderMode = mode;
}

bool Renderer::loadShaders() {
    // Load solid shaders from files
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

    GLuint vSolid = compileShader(vertexCode, GL_VERTEX_SHADER);
    GLuint fSolid = compileShader(fragmentCode, GL_FRAGMENT_SHADER);
    if (!vSolid || !fSolid) return false;
    m_shaderProgramSolid = linkProgram(vSolid, fSolid);
    glDeleteShader(vSolid);
    glDeleteShader(fSolid);
    if (!m_shaderProgramSolid) return false;

    // Create a minimal wireframe fragment shader (pure white) using the same vertex shader
    const char* wireFrag = R"(
        #version 330 core
        out vec4 FragColor;
        void main() {
            FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        }
    )";
    GLuint vWire = compileShader(vertexCode, GL_VERTEX_SHADER);
    GLuint fWire = compileShader(std::string(wireFrag), GL_FRAGMENT_SHADER);
    if (!vWire || !fWire) return false;
    m_shaderProgramWireframe = linkProgram(vWire, fWire);
    glDeleteShader(vWire);
    glDeleteShader(fWire);
    if (!m_shaderProgramWireframe) return false;

    return true;
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
