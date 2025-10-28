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
    void setDrawFacetNormals(bool enabled) { m_drawFacetNormals = enabled; }
    bool getDrawFacetNormals() const { return m_drawFacetNormals; }
    void setNormalLengthScale(float s) { m_normalLengthScale = s; }
    float getNormalLengthScale() const { return m_normalLengthScale; }
    
    Mesh* getMesh() const { return m_mesh.get(); }
    
private:
    bool loadShaders();
    GLuint compileShader(const std::string& source, GLenum type);
    GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader);
    void setupMesh();
    void setupFacetNormals();
    
    std::unique_ptr<Mesh> m_mesh;
    GLuint m_VAO;
    GLuint m_VBO;
    GLuint m_EBO;       // Element buffer for triangles (solid mode)
    GLuint m_edgeEBO;   // Element buffer for edges (wireframe mode)
    GLuint m_shaderProgramSolid;
    GLuint m_shaderProgramWireframe;
    // Debug: facet normals
    GLuint m_normalsVAO;
    GLuint m_normalsVBO;
    GLuint m_shaderProgramNormals;
    GLsizei m_normalsVertexCount; // number of vertices (2 per facet)
    bool m_drawFacetNormals;
    float m_normalLengthScale; // relative to model extent
    RenderMode m_renderMode;
    size_t m_indexCount;     // Number of triangle indices for rendering
    size_t m_edgeIndexCount; // Number of edge indices for wireframe
};
