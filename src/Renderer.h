#pragma once

#include "Mesh.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>

class Renderer {
public:
    Renderer();
    ~Renderer();
    
    bool initialize();
    void setMesh(std::unique_ptr<Mesh> mesh);
    void render(const glm::mat4& projection, const glm::mat4& view, const glm::mat4& model);
    // Draw toggles
    void setDrawSolid(bool enabled) { m_drawSolid = enabled; }
    bool getDrawSolid() const { return m_drawSolid; }
    void setDrawWireframe(bool enabled) { m_drawWireframe = enabled; }
    bool getDrawWireframe() const { return m_drawWireframe; }
    void setDrawFacetNormals(bool enabled) { m_drawFacetNormals = enabled; }
    bool getDrawFacetNormals() const { return m_drawFacetNormals; }
    void setNormalLengthScale(float s) { m_normalLengthScale = s; }
    float getNormalLengthScale() const { return m_normalLengthScale; }
    void setCullingEnabled(bool enabled) { m_cullingEnabled = enabled; }
    bool getCullingEnabled() const { return m_cullingEnabled; }
    
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
    // Debug: triangle normals
    GLuint m_triNormalsVAO;
    GLuint m_triNormalsVBO;
    GLsizei m_triNormalsVertexCount; // number of vertices (2 per triangle)
    // Debug: triangle edges (from triangulation)
    GLuint m_triEdgesVAO;
    GLuint m_triEdgesVBO;
    GLsizei m_triEdgesVertexCount; // number of vertices (2 per edge * 3 per tri)
    // Solid-mode dedicated VBO/VAO with per-triangle facet normals to hide triangulation
    GLuint m_solidVAO;
    GLuint m_solidVBO;
    GLsizei m_solidVertexCount; // number of vertices for glDrawArrays
    bool m_drawFacetNormals;
    float m_normalLengthScale; // relative to model extent
    bool m_cullingEnabled; // back-face culling toggle
    bool m_drawSolid;     // draw solid triangles
    bool m_drawWireframe; // draw wireframe edges
    size_t m_indexCount;     // Number of triangle indices for rendering
    size_t m_edgeIndexCount; // Number of edge indices for wireframe
};
