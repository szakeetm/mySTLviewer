#pragma once

#include <glm/glm.hpp>
#include <vector>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct Triangle {
    glm::vec3 normal;
    glm::vec3 vertices[3];
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    glm::vec3 min_bounds;
    glm::vec3 max_bounds;
    
    void calculateBounds() {
        if (vertices.empty()) return;
        
        min_bounds = vertices[0].position;
        max_bounds = vertices[0].position;
        
        for (const auto& v : vertices) {
            min_bounds = glm::min(min_bounds, v.position);
            max_bounds = glm::max(max_bounds, v.position);
        }
    }
    
    glm::vec3 getCenter() const {
        return (min_bounds + max_bounds) * 0.5f;
    }
    
    float getMaxExtent() const {
        glm::vec3 extent = max_bounds - min_bounds;
        return glm::max(glm::max(extent.x, extent.y), extent.z);
    }
};
