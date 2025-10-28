#include "Renderer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>
#include <mapbox/earcut.hpp>

Renderer::Renderer()
    : m_VAO(0), m_VBO(0), m_EBO(0), m_edgeEBO(0),
      m_shaderProgramSolid(0), m_shaderProgramWireframe(0),
      m_normalsVAO(0), m_normalsVBO(0), m_shaderProgramNormals(0),
      m_normalsVertexCount(0), m_drawFacetNormals(false), m_normalLengthScale(0.03f),
      m_renderMode(RenderMode::WIREFRAME), m_indexCount(0), m_edgeIndexCount(0) {
}

Renderer::~Renderer() {
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_EBO) glDeleteBuffers(1, &m_EBO);
    if (m_edgeEBO) glDeleteBuffers(1, &m_edgeEBO);
    if (m_shaderProgramSolid) glDeleteProgram(m_shaderProgramSolid);
    if (m_shaderProgramWireframe) glDeleteProgram(m_shaderProgramWireframe);
    if (m_normalsVAO) glDeleteVertexArrays(1, &m_normalsVAO);
    if (m_normalsVBO) glDeleteBuffers(1, &m_normalsVBO);
    if (m_shaderProgramNormals) glDeleteProgram(m_shaderProgramNormals);
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
    if (m_edgeEBO) glDeleteBuffers(1, &m_edgeEBO);
    if (m_normalsVAO) { glDeleteVertexArrays(1, &m_normalsVAO); m_normalsVAO = 0; }
    if (m_normalsVBO) { glDeleteBuffers(1, &m_normalsVBO); m_normalsVBO = 0; }
    
    // Generate buffers
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);
    glGenBuffers(1, &m_edgeEBO);
    
    glBindVertexArray(m_VAO);
    
    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_mesh->vertices.size() * sizeof(Vertex),
                 m_mesh->vertices.data(), GL_STATIC_DRAW);
    
    // Convert facets to triangle indices using earcut for proper triangulation
    std::vector<unsigned int> triangleIndices;
    
    for (const auto& facet : m_mesh->facets) {
        if (facet.indices.size() < 3) {
            continue; // Skip degenerate facets
        }
        
        // For triangles, just add the indices directly
        if (facet.indices.size() == 3) {
            triangleIndices.push_back(facet.indices[0]);
            triangleIndices.push_back(facet.indices[1]);
            triangleIndices.push_back(facet.indices[2]);
        } else {
            // For polygons with more than 3 vertices, use earcut for proper triangulation
            // Earcut expects a vector of polygons, where each polygon is a vector of points
            // Each point is an array-like structure with at least 2 coordinates
            
            // Build the polygon for earcut (2D projection - use first two components)
            using Point = std::array<double, 2>;
            std::vector<std::vector<Point>> polygon;
            std::vector<Point> ring;
            
            for (unsigned int idx : facet.indices) {
                const auto& pos = m_mesh->vertices[idx].position;
                // Project to 2D using X and Y coordinates
                // TODO: For better results, project onto the plane defined by the facet normal
                ring.push_back({static_cast<double>(pos.x), static_cast<double>(pos.y)});
            }
            
            polygon.push_back(ring);
            
            // Run earcut triangulation
            std::vector<unsigned int> localIndices = mapbox::earcut<unsigned int>(polygon);
            
            // Add the triangulated indices (offset by the facet's base indices)
            for (unsigned int localIdx : localIndices) {
                triangleIndices.push_back(facet.indices[localIdx]);
            }
        }
    }
    
    // Store the number of indices for rendering
    m_indexCount = triangleIndices.size();
    
    // Upload triangle index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, triangleIndices.size() * sizeof(unsigned int),
                 triangleIndices.data(), GL_STATIC_DRAW);
    
    // Build edge indices for wireframe (original facet edges only, no triangulation)
    std::vector<unsigned int> edgeIndices;
    for (const auto& facet : m_mesh->facets) {
        if (facet.indices.size() < 2) {
            continue; // Skip degenerate facets
        }
        
        // Create edges around the perimeter of the facet
        for (size_t i = 0; i < facet.indices.size(); ++i) {
            edgeIndices.push_back(facet.indices[i]);
            edgeIndices.push_back(facet.indices[(i + 1) % facet.indices.size()]);
        }
    }
    
    m_edgeIndexCount = edgeIndices.size();
    
    // Upload edge index data to separate buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_edgeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, edgeIndices.size() * sizeof(unsigned int),
                 edgeIndices.data(), GL_STATIC_DRAW);
    
    // Bind back to triangle EBO as default
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    
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
              << " vertices, " << m_mesh->facets.size() << " facets, " 
              << m_indexCount / 3 << " triangles" << std::endl;

    // Build facet normals debug geometry
    setupFacetNormals();
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
    
    glBindVertexArray(m_VAO);
    
    if (m_renderMode == RenderMode::WIREFRAME) {
        // Wireframe: draw edges using GL_LINES
        glLineWidth(1.5f);
        glDisable(GL_CULL_FACE);
        
        // Bind the edge buffer and draw lines
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_edgeEBO);
        glDrawElements(GL_LINES, m_edgeIndexCount, GL_UNSIGNED_INT, 0);
    } else {
        // Solid: draw triangles
        glDisable(GL_LINE_SMOOTH);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        
        // Bind the triangle buffer and draw
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
        glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
    }
    
    glBindVertexArray(0);

    // Optional: draw facet normals for debugging
    if (m_drawFacetNormals && m_shaderProgramNormals && m_normalsVAO && m_normalsVertexCount > 0) {
        GLboolean wasDepth = glIsEnabled(GL_DEPTH_TEST);
        // Draw on top so you can always see them
        glDisable(GL_DEPTH_TEST);
        glUseProgram(m_shaderProgramNormals);
        GLint projLoc = glGetUniformLocation(m_shaderProgramNormals, "projection");
        GLint viewLoc = glGetUniformLocation(m_shaderProgramNormals, "view");
        GLint modelLoc = glGetUniformLocation(m_shaderProgramNormals, "model");
        GLint colorLoc = glGetUniformLocation(m_shaderProgramNormals, "color");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        // Magenta for visibility
        glUniform3f(colorLoc, 1.0f, 0.0f, 1.0f);
        glBindVertexArray(m_normalsVAO);
        glLineWidth(1.5f);
        glDrawArrays(GL_LINES, 0, m_normalsVertexCount);
        glBindVertexArray(0);
        if (wasDepth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    }
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

    // Create a simple shader for drawing facet normals as lines with uniform color
    const char* normalsVS = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        uniform mat4 projection;
        uniform mat4 view;
        uniform mat4 model;
        void main(){
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )";
    const char* normalsFS = R"(
        #version 330 core
        uniform vec3 color;
        out vec4 FragColor;
        void main(){
            FragColor = vec4(color, 1.0);
        }
    )";
    GLuint vNorm = compileShader(std::string(normalsVS), GL_VERTEX_SHADER);
    GLuint fNorm = compileShader(std::string(normalsFS), GL_FRAGMENT_SHADER);
    if (!vNorm || !fNorm) return false;
    m_shaderProgramNormals = linkProgram(vNorm, fNorm);
    glDeleteShader(vNorm);
    glDeleteShader(fNorm);
    if (!m_shaderProgramNormals) return false;

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

void Renderer::setupFacetNormals() {
    m_normalsVertexCount = 0;
    if (!m_mesh || m_mesh->facets.empty()) return;

    // Compute scale from mesh extent
    float length = glm::max(0.001f, m_mesh->getMaxExtent() * m_normalLengthScale);

    std::vector<glm::vec3> lineVerts;
    lineVerts.reserve(m_mesh->facets.size() * 2);

    for (const auto& facet : m_mesh->facets) {
        if (facet.indices.size() < 3) continue;
        // Compute centroid
        glm::vec3 centroid(0.0f);
        for (unsigned int idx : facet.indices) {
            centroid += m_mesh->vertices[idx].position;
        }
        centroid /= static_cast<float>(facet.indices.size());
        // Compute normal using Newell's method for robustness
        glm::vec3 normal(0.0f);
        for (size_t i = 0; i < facet.indices.size(); ++i) {
            const glm::vec3& v1 = m_mesh->vertices[facet.indices[i]].position;
            const glm::vec3& v2 = m_mesh->vertices[facet.indices[(i + 1) % facet.indices.size()]].position;
            normal.x += (v1.y - v2.y) * (v1.z + v2.z);
            normal.y += (v1.z - v2.z) * (v1.x + v2.x);
            normal.z += (v1.x - v2.x) * (v1.y + v2.y);
        }
        float len = glm::length(normal);
        if (len > 1e-6f) normal /= len; else {
            // Fallback to cross of first two edges
            const glm::vec3& a = m_mesh->vertices[facet.indices[0]].position;
            const glm::vec3& b = m_mesh->vertices[facet.indices[1]].position;
            const glm::vec3& c = m_mesh->vertices[facet.indices[2]].position;
            normal = glm::normalize(glm::cross(b - a, c - a));
        }
        lineVerts.push_back(centroid);
        lineVerts.push_back(centroid + normal * length);
    }

    if (lineVerts.empty()) return;

    // Create VAO/VBO for lines
    glGenVertexArrays(1, &m_normalsVAO);
    glGenBuffers(1, &m_normalsVBO);
    glBindVertexArray(m_normalsVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_normalsVBO);
    glBufferData(GL_ARRAY_BUFFER, lineVerts.size() * sizeof(glm::vec3), lineVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    m_normalsVertexCount = static_cast<GLsizei>(lineVerts.size());
}
