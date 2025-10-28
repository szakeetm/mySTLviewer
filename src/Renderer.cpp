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
        m_normalsVertexCount(0),
        m_triNormalsVAO(0), m_triNormalsVBO(0), m_triNormalsVertexCount(0),
        m_triEdgesVAO(0), m_triEdgesVBO(0), m_triEdgesVertexCount(0),
        m_solidVAO(0), m_solidVBO(0), m_solidVertexCount(0),
        m_drawFacetNormals(false), m_normalLengthScale(0.03f), m_cullingEnabled(false),
            m_drawSolid(true), m_drawWireframe(false), m_indexCount(0), m_edgeIndexCount(0) {
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
    if (m_triNormalsVAO) glDeleteVertexArrays(1, &m_triNormalsVAO);
    if (m_triNormalsVBO) glDeleteBuffers(1, &m_triNormalsVBO);
    if (m_triEdgesVAO) glDeleteVertexArrays(1, &m_triEdgesVAO);
    if (m_triEdgesVBO) glDeleteBuffers(1, &m_triEdgesVBO);
    if (m_solidVAO) glDeleteVertexArrays(1, &m_solidVAO);
    if (m_solidVBO) glDeleteBuffers(1, &m_solidVBO);
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
    if (m_triNormalsVAO) { glDeleteVertexArrays(1, &m_triNormalsVAO); m_triNormalsVAO = 0; }
    if (m_triNormalsVBO) { glDeleteBuffers(1, &m_triNormalsVBO); m_triNormalsVBO = 0; }
    if (m_triEdgesVAO) { glDeleteVertexArrays(1, &m_triEdgesVAO); m_triEdgesVAO = 0; }
    if (m_triEdgesVBO) { glDeleteBuffers(1, &m_triEdgesVBO); m_triEdgesVBO = 0; }
    if (m_solidVAO) { glDeleteVertexArrays(1, &m_solidVAO); m_solidVAO = 0; }
    if (m_solidVBO) { glDeleteBuffers(1, &m_solidVBO); m_solidVBO = 0; }
    
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
    struct SolidVertex { glm::vec3 position; glm::vec3 facetNormal; glm::vec3 facetCenter; };
    std::vector<SolidVertex> solidVertices;

    for (const auto& facet : m_mesh->facets) {
        const size_t n = facet.indices.size();
        if (n < 3) continue; // Skip degenerate facets

        // Compute facet centroid
        glm::vec3 facetCenter(0.0f);
        for (unsigned int idx : facet.indices) {
            facetCenter += m_mesh->vertices[idx].position;
        }
        facetCenter /= static_cast<float>(n);

        // Compute a robust facet normal via Newell's method
        glm::vec3 facetNormal(0.0f);
        for (size_t i = 0; i < n; ++i) {
            const glm::vec3& v1 = m_mesh->vertices[facet.indices[i]].position;
            const glm::vec3& v2 = m_mesh->vertices[facet.indices[(i + 1) % n]].position;
            facetNormal.x += (v1.y - v2.y) * (v1.z + v2.z);
            facetNormal.y += (v1.z - v2.z) * (v1.x + v2.x);
            facetNormal.z += (v1.x - v2.x) * (v1.y + v2.y);
        }
        if (glm::length(facetNormal) < 1e-8f) {
            // Fallback to first triangle cross if Newell is degenerate
            const glm::vec3 a = m_mesh->vertices[facet.indices[0]].position;
            const glm::vec3 b = m_mesh->vertices[facet.indices[1]].position;
            const glm::vec3 c = m_mesh->vertices[facet.indices[2]].position;
            facetNormal = glm::cross(b - a, c - a);
        }
        if (glm::length(facetNormal) > 1e-8f) facetNormal = glm::normalize(facetNormal);

        auto appendOrientedTri = [&](unsigned int i0, unsigned int i1, unsigned int i2) {
            const glm::vec3& p0 = m_mesh->vertices[i0].position;
            const glm::vec3& p1 = m_mesh->vertices[i1].position;
            const glm::vec3& p2 = m_mesh->vertices[i2].position;
            glm::vec3 triN = glm::cross(p1 - p0, p2 - p0);
            if (glm::length(triN) > 1e-12f && glm::dot(triN, facetNormal) < 0.0f) {
                // Flip winding to match facet normal
                std::swap(i1, i2);
            }
            triangleIndices.push_back(i0);
            triangleIndices.push_back(i1);
            triangleIndices.push_back(i2);
            // Push de-indexed solid vertices with facet normal AND facet center
            solidVertices.push_back({ m_mesh->vertices[i0].position, facetNormal, facetCenter });
            solidVertices.push_back({ m_mesh->vertices[i1].position, facetNormal, facetCenter });
            solidVertices.push_back({ m_mesh->vertices[i2].position, facetNormal, facetCenter });
        };

        if (n == 3) {
            appendOrientedTri(facet.indices[0], facet.indices[1], facet.indices[2]);
        } else {
            // For polygons with more than 3 vertices, triangulate by projecting onto the facet plane
            using Point = std::array<double, 2>;
            std::vector<std::vector<Point>> polygon;
            std::vector<Point> ring;
            ring.reserve(n);

            // Build a local 2D basis on the facet plane
            // Choose an up vector not parallel to facetNormal
            glm::vec3 up = (std::abs(facetNormal.z) < 0.9f) ? glm::vec3(0,0,1) : glm::vec3(0,1,0);
            glm::vec3 tangent = glm::normalize(glm::cross(up, facetNormal));
            glm::vec3 bitangent = glm::normalize(glm::cross(facetNormal, tangent));
            // Use centroid as origin for numerical stability
            glm::vec3 centroid(0.0f);
            for (unsigned int idx : facet.indices) centroid += m_mesh->vertices[idx].position;
            centroid /= static_cast<float>(n);
            for (unsigned int idx : facet.indices) {
                glm::vec3 p = m_mesh->vertices[idx].position - centroid;
                double u = static_cast<double>(glm::dot(p, tangent));
                double v = static_cast<double>(glm::dot(p, bitangent));
                ring.push_back({u, v});
            }
            polygon.push_back(ring);

            std::vector<unsigned int> localIndices = mapbox::earcut<unsigned int>(polygon);

            if (localIndices.size() < (n - 2) * 3) {
                // Fallback: simple triangle fan around vertex 0
                for (size_t j = 1; j + 1 < n; ++j) {
                    unsigned int gi0 = facet.indices[0];
                    unsigned int gi1 = facet.indices[j];
                    unsigned int gi2 = facet.indices[j + 1];
                    appendOrientedTri(gi0, gi1, gi2);
                }
            } else {
                // Map local earcut indices back to the facet's global vertex indices
                for (size_t k = 0; k + 2 < localIndices.size(); k += 3) {
                    unsigned int gi0 = facet.indices[localIndices[k + 0]];
                    unsigned int gi1 = facet.indices[localIndices[k + 1]];
                    unsigned int gi2 = facet.indices[localIndices[k + 2]];
                    appendOrientedTri(gi0, gi1, gi2);
                }
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

    // Build triangle normals debug geometry (from triangulated indices)
    m_triNormalsVertexCount = 0;
    m_triEdgesVertexCount = 0;
    if (!triangleIndices.empty()) {
        float length = glm::max(0.001f, m_mesh->getMaxExtent() * m_normalLengthScale);
        std::vector<glm::vec3> triLineVerts;
        std::vector<glm::vec3> triEdgeVerts;
        triLineVerts.reserve((triangleIndices.size() / 3) * 2);
        triEdgeVerts.reserve((triangleIndices.size() / 3) * 6);
        for (size_t i = 0; i + 2 < triangleIndices.size(); i += 3) {
            unsigned int i0 = triangleIndices[i + 0];
            unsigned int i1 = triangleIndices[i + 1];
            unsigned int i2 = triangleIndices[i + 2];
            const glm::vec3& p0 = m_mesh->vertices[i0].position;
            const glm::vec3& p1 = m_mesh->vertices[i1].position;
            const glm::vec3& p2 = m_mesh->vertices[i2].position;
            glm::vec3 triN = glm::cross(p1 - p0, p2 - p0);
            float ln = glm::length(triN);
            if (ln > 1e-12f) triN /= ln; else continue;
            glm::vec3 centroid = (p0 + p1 + p2) / 3.0f;
            triLineVerts.push_back(centroid);
            triLineVerts.push_back(centroid + triN * length);
            // Triangle edges (3 segments)
            triEdgeVerts.push_back(p0); triEdgeVerts.push_back(p1);
            triEdgeVerts.push_back(p1); triEdgeVerts.push_back(p2);
            triEdgeVerts.push_back(p2); triEdgeVerts.push_back(p0);
        }
        if (!triLineVerts.empty()) {
            glGenVertexArrays(1, &m_triNormalsVAO);
            glGenBuffers(1, &m_triNormalsVBO);
            glBindVertexArray(m_triNormalsVAO);
            glBindBuffer(GL_ARRAY_BUFFER, m_triNormalsVBO);
            glBufferData(GL_ARRAY_BUFFER, triLineVerts.size() * sizeof(glm::vec3), triLineVerts.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
            glEnableVertexAttribArray(0);
            glBindVertexArray(0);
            m_triNormalsVertexCount = static_cast<GLsizei>(triLineVerts.size());
        }
        if (!triEdgeVerts.empty()) {
            glGenVertexArrays(1, &m_triEdgesVAO);
            glGenBuffers(1, &m_triEdgesVBO);
            glBindVertexArray(m_triEdgesVAO);
            glBindBuffer(GL_ARRAY_BUFFER, m_triEdgesVBO);
            glBufferData(GL_ARRAY_BUFFER, triEdgeVerts.size() * sizeof(glm::vec3), triEdgeVerts.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
            glEnableVertexAttribArray(0);
            glBindVertexArray(0);
            m_triEdgesVertexCount = static_cast<GLsizei>(triEdgeVerts.size());
        }
    }

    // Build solid-mode VBO/VAO (positions + facet normals + facet centers), draw with glDrawArrays
    m_solidVertexCount = static_cast<GLsizei>(solidVertices.size());
    if (m_solidVertexCount > 0) {
        glGenVertexArrays(1, &m_solidVAO);
        glGenBuffers(1, &m_solidVBO);
        glBindVertexArray(m_solidVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_solidVBO);
        glBufferData(GL_ARRAY_BUFFER, solidVertices.size() * sizeof(SolidVertex), solidVertices.data(), GL_STATIC_DRAW);
        // position at location 0
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SolidVertex), (void*)offsetof(SolidVertex, position));
        glEnableVertexAttribArray(0);
        // facet normal at location 1
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SolidVertex), (void*)offsetof(SolidVertex, facetNormal));
        glEnableVertexAttribArray(1);
        // facet center at location 2
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(SolidVertex), (void*)offsetof(SolidVertex, facetCenter));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);
    }
}

void Renderer::render(const glm::mat4& projection, const glm::mat4& view, const glm::mat4& model) {
    if (!m_mesh || m_mesh->vertices.empty()) {
        return;
    }

    // Pass 1: Solid fill (if enabled)
    if (m_drawSolid && m_shaderProgramSolid) {
        glUseProgram(m_shaderProgramSolid);
        GLint projLoc = glGetUniformLocation(m_shaderProgramSolid, "projection");
        GLint viewLoc = glGetUniformLocation(m_shaderProgramSolid, "view");
        GLint modelLoc = glGetUniformLocation(m_shaderProgramSolid, "model");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        glDisable(GL_LINE_SMOOTH);
        glDisable(GL_BLEND);
        if (m_cullingEnabled) {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
        } else {
            glDisable(GL_CULL_FACE);
        }

        if (m_solidVAO && m_solidVertexCount > 0) {
            glBindVertexArray(m_solidVAO);
            glDrawArrays(GL_TRIANGLES, 0, m_solidVertexCount);
            glBindVertexArray(0);
        } else {
            glBindVertexArray(m_VAO);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
            glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
    }

    // Pass 2: Wireframe overlay (if enabled)
    if (m_drawWireframe && m_shaderProgramWireframe) {
        glUseProgram(m_shaderProgramWireframe);
        GLint projLoc = glGetUniformLocation(m_shaderProgramWireframe, "projection");
        GLint viewLoc = glGetUniformLocation(m_shaderProgramWireframe, "view");
        GLint modelLoc = glGetUniformLocation(m_shaderProgramWireframe, "model");
        GLint colorLoc = glGetUniformLocation(m_shaderProgramWireframe, "lineColor");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        
        // White when wireframe-only, black when overlaid on solid
        if (m_drawSolid) {
            glUniform3f(colorLoc, 0.0f, 0.0f, 0.0f); // black overlay
        } else {
            glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f); // white wireframe
        }

        glLineWidth(1.5f);
        glDisable(GL_CULL_FACE);
        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_edgeEBO);
        glDrawElements(GL_LINES, m_edgeIndexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // Optional: draw normals for debugging
    if (m_drawFacetNormals && m_shaderProgramNormals) {
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
        glLineWidth(1.5f);
        // Facet normals: magenta
        if (m_normalsVAO && m_normalsVertexCount > 0) {
            glUniform3f(colorLoc, 1.0f, 0.0f, 1.0f);
            glBindVertexArray(m_normalsVAO);
            glDrawArrays(GL_LINES, 0, m_normalsVertexCount);
            glBindVertexArray(0);
        }
        // Triangle normals: cyan
        if (m_triNormalsVAO && m_triNormalsVertexCount > 0) {
            glUniform3f(colorLoc, 0.0f, 1.0f, 1.0f);
            glBindVertexArray(m_triNormalsVAO);
            glDrawArrays(GL_LINES, 0, m_triNormalsVertexCount);
            glBindVertexArray(0);
        }
        // Triangle edges: yellow
        if (m_triEdgesVAO && m_triEdgesVertexCount > 0) {
            glUniform3f(colorLoc, 1.0f, 1.0f, 0.0f);
            glBindVertexArray(m_triEdgesVAO);
            glDrawArrays(GL_LINES, 0, m_triEdgesVertexCount);
            glBindVertexArray(0);
        }
        if (wasDepth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    }
}

// setRenderMode removed in favor of independent toggles

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

    // Load wireframe shaders from files
    std::ifstream wireFragFile("shaders/wireframe.frag");
    if (!wireFragFile.is_open()) {
        std::cerr << "Failed to open wireframe fragment shader file" << std::endl;
        return false;
    }
    std::stringstream wireFragStream;
    wireFragStream << wireFragFile.rdbuf();
    std::string wireFragCode = wireFragStream.str();

    GLuint vWire = compileShader(vertexCode, GL_VERTEX_SHADER);
    GLuint fWire = compileShader(wireFragCode, GL_FRAGMENT_SHADER);
    if (!vWire || !fWire) return false;
    m_shaderProgramWireframe = linkProgram(vWire, fWire);
    glDeleteShader(vWire);
    glDeleteShader(fWire);
    if (!m_shaderProgramWireframe) return false;

    // Load normals debug shaders from files
    std::ifstream normalsVSFile("shaders/normals.vert");
    std::ifstream normalsFSFile("shaders/normals.frag");
    if (!normalsVSFile.is_open() || !normalsFSFile.is_open()) {
        std::cerr << "Failed to open normals shader files" << std::endl;
        return false;
    }
    std::stringstream normalsVSStream, normalsFSStream;
    normalsVSStream << normalsVSFile.rdbuf();
    normalsFSStream << normalsFSFile.rdbuf();
    std::string normalsVSCode = normalsVSStream.str();
    std::string normalsFSCode = normalsFSStream.str();

    GLuint vNorm = compileShader(normalsVSCode, GL_VERTEX_SHADER);
    GLuint fNorm = compileShader(normalsFSCode, GL_FRAGMENT_SHADER);
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
