#include "STLLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>

std::unique_ptr<Mesh> STLLoader::load(const std::string& filename) {
    if (isBinarySTL(filename)) {
        return loadBinary(filename);
    } else {
        return loadASCII(filename);
    }
}

bool STLLoader::isBinarySTL(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Read first 80 bytes (header)
    char header[80];
    file.read(header, 80);
    
    // Check if "solid" appears in ASCII format
    std::string headerStr(header, 80);
    if (headerStr.find("solid") == 0) {
        // Might be ASCII, check further
        // Try to read the number of triangles for binary
        file.seekg(80);
        uint32_t numTriangles;
        file.read(reinterpret_cast<char*>(&numTriangles), 4);
        
        // Check if file size matches binary format
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        size_t expectedSize = 80 + 4 + (numTriangles * 50);
        
        return (fileSize == expectedSize);
    }
    
    return true; // Likely binary
}

std::unique_ptr<Mesh> STLLoader::loadBinary(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return nullptr;
    }
    
    auto mesh = std::make_unique<Mesh>();
    
    // Skip 80-byte header
    file.seekg(80);
    
    // Read number of triangles
    uint32_t numTriangles;
    file.read(reinterpret_cast<char*>(&numTriangles), 4);
    
    std::cout << "Loading binary STL with " << numTriangles << " triangles..." << std::endl;
    
    mesh->vertices.reserve(numTriangles * 3);
    mesh->indices.reserve(numTriangles * 3);
    
    for (uint32_t i = 0; i < numTriangles; ++i) {
        // Read normal
        float normal[3];
        file.read(reinterpret_cast<char*>(normal), 12);
        glm::vec3 n(normal[0], normal[1], normal[2]);
        
        // Read 3 vertices
        for (int j = 0; j < 3; ++j) {
            float vertex[3];
            file.read(reinterpret_cast<char*>(vertex), 12);
            
            Vertex v;
            v.position = glm::vec3(vertex[0], vertex[1], vertex[2]);
            v.normal = n;
            
            mesh->vertices.push_back(v);
            mesh->indices.push_back(i * 3 + j);
        }
        
        // Skip attribute byte count
        file.seekg(2, std::ios::cur);
    }
    
    mesh->calculateBounds();
    std::cout << "Loaded " << mesh->vertices.size() << " vertices" << std::endl;
    
    return mesh;
}

std::unique_ptr<Mesh> STLLoader::loadASCII(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return nullptr;
    }
    
    auto mesh = std::make_unique<Mesh>();
    std::string line;
    glm::vec3 currentNormal;
    std::vector<glm::vec3> currentTriangle;
    
    std::cout << "Loading ASCII STL..." << std::endl;
    
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;
        
        if (keyword == "facet") {
            std::string normalStr;
            iss >> normalStr; // "normal"
            float nx, ny, nz;
            iss >> nx >> ny >> nz;
            currentNormal = glm::vec3(nx, ny, nz);
            currentTriangle.clear();
        }
        else if (keyword == "vertex") {
            float x, y, z;
            iss >> x >> y >> z;
            currentTriangle.push_back(glm::vec3(x, y, z));
        }
        else if (keyword == "endfacet") {
            if (currentTriangle.size() == 3) {
                unsigned int baseIndex = mesh->vertices.size();
                for (const auto& pos : currentTriangle) {
                    Vertex v;
                    v.position = pos;
                    v.normal = currentNormal;
                    mesh->vertices.push_back(v);
                }
                mesh->indices.push_back(baseIndex);
                mesh->indices.push_back(baseIndex + 1);
                mesh->indices.push_back(baseIndex + 2);
            }
        }
    }
    
    mesh->calculateBounds();
    std::cout << "Loaded " << mesh->vertices.size() << " vertices" << std::endl;
    
    return mesh;
}
