#pragma once

#include "Mesh.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>

enum class RenderMode {
    SOLID,
    WIREFRAME
};

class Renderer {
public:
    Renderer();
    ~Renderer();
    
    bool initialize();
    void setMesh(std::unique_ptr<Mesh> mesh);
    void render(const glm::mat4& projection, const glm::mat4& view, const glm::mat4& model);
    void setRenderMode(RenderMode mode);
    RenderMode getRenderMode() const { return m_renderMode; }
    
    Mesh* getMesh() const { return m_mesh.get(); }
    
private:
    bool loadShaders();
    GLuint compileShader(const std::string& source, GLenum type);
    GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader);
    void setupMesh();
    
    std::unique_ptr<Mesh> m_mesh;
    GLuint m_VAO;
    GLuint m_VBO;
    GLuint m_EBO;
    GLuint m_shaderProgramSolid;
    GLuint m_shaderProgramWireframe;
    RenderMode m_renderMode;
};
