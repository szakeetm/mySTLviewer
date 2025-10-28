#include "Renderer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>
#include <mapbox/earcut.hpp>

Renderer::Renderer()
    : m_VAO(0), m_VBO(0), m_EBO(0), m_edgeEBO(0), m_shaderProgramSolid(0), m_shaderProgramWireframe(0), m_renderMode(RenderMode::WIREFRAME), m_indexCount(0), m_edgeIndexCount(0) {
}

Renderer::~Renderer() {
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_EBO) glDeleteBuffers(1, &m_EBO);
    if (m_edgeEBO) glDeleteBuffers(1, &m_edgeEBO);
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
    if (m_edgeEBO) glDeleteBuffers(1, &m_edgeEBO);
    
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
