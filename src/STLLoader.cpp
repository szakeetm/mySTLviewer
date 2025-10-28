#include "STLLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstring>

// Simple console progress bar
static void printProgressBar(const std::string& prefix, float fraction) {
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    const int width = 40; // progress bar width in chars
    int pos = static_cast<int>(fraction * width + 0.5f);
    std::cout << "\r" << prefix << " [";
    for (int i = 0; i < width; ++i) {
        std::cout << (i < pos ? '=' : ' ');
    }
    std::cout << "] " << std::fixed << std::setprecision(1) << (fraction * 100.0f) << "%";
    std::cout.flush();
}

// (Removed outward-winding/normal fix; renderer debug adjusts visualization only.)

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
    mesh->facets.reserve(numTriangles);
    
    // Progress tracking
    uint32_t lastPercent = 0;
    if (numTriangles == 0) {
        printProgressBar("Reading triangles", 1.0f);
    }

    for (uint32_t i = 0; i < numTriangles; ++i) {
        // Skip normal (we'll compute from winding)
        file.seekg(12, std::ios::cur);
        
        // Read 3 vertices
        glm::vec3 positions[3];
        for (int j = 0; j < 3; ++j) {
            float vertex[3];
            file.read(reinterpret_cast<char*>(vertex), 12);
            positions[j] = glm::vec3(vertex[0], vertex[1], vertex[2]);
        }
        
        // Compute normal from winding order
        glm::vec3 n = glm::cross(positions[1] - positions[0], positions[2] - positions[0]);
        if (glm::length(n) > 1e-12f) {
            n = glm::normalize(n);
        } else {
            n = glm::vec3(0, 0, 1); // fallback
        }
        
        // Create vertices with computed normal
        unsigned int baseIndex = mesh->vertices.size();
        for (int j = 0; j < 3; ++j) {
            Vertex v;
            v.position = positions[j];
            v.normal = n;
            mesh->vertices.push_back(v);
        }
        
        // Create a triangular facet with flipped winding order
        mesh->facets.push_back(Facet{baseIndex, baseIndex + 2, baseIndex + 1});
        
        // Skip attribute byte count
        file.seekg(2, std::ios::cur);

        // Update progress bar only when percentage changes (max 100 prints)
        uint32_t percent = numTriangles ? static_cast<uint32_t>(((i + 1) * 100ULL) / numTriangles) : 100;
        if (percent != lastPercent) {
            lastPercent = percent;
            printProgressBar("Reading triangles", percent / 100.0f);
        }
    }
    
    mesh->calculateBounds();
    std::cout << "\nLoaded " << mesh->vertices.size() << " vertices" << std::endl;
    
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
    
    // Determine total file size (bytes) for progress estimation
    std::ifstream fsize(filename, std::ios::binary);
    std::streampos totalBytes = 0;
    if (fsize.is_open()) {
        fsize.seekg(0, std::ios::end);
        totalBytes = fsize.tellg();
    }
    std::streampos approxRead = 0;
    uint32_t lastPercent = 0;
    
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;
        
        if (keyword == "facet") {
            std::string normalStr;
            iss >> normalStr; // "normal"
            float nx, ny, nz;
            iss >> nx >> ny >> nz;
            // Skip normal, we'll compute from winding
            currentTriangle.clear();
        }
        else if (keyword == "vertex") {
            float x, y, z;
            iss >> x >> y >> z;
            currentTriangle.push_back(glm::vec3(x, y, z));
        }
        else if (keyword == "endfacet") {
            if (currentTriangle.size() == 3) {
                // Compute normal from winding order
                glm::vec3 n = glm::cross(currentTriangle[1] - currentTriangle[0], currentTriangle[2] - currentTriangle[0]);
                if (glm::length(n) > 1e-12f) {
                    n = glm::normalize(n);
                } else {
                    n = glm::vec3(0, 0, 1); // fallback
                }
                
                unsigned int baseIndex = mesh->vertices.size();
                for (const auto& pos : currentTriangle) {
                    Vertex v;
                    v.position = pos;
                    v.normal = n;
                    mesh->vertices.push_back(v);
                }
                // Create a triangular facet with flipped winding order
                mesh->facets.push_back(Facet{baseIndex, baseIndex + 2, baseIndex + 1});
            }
        }
        // Progress update by file position when available, else by approximate bytes
        std::streampos pos = file.tellg();
        if (pos == std::streampos(-1)) {
            approxRead += static_cast<std::streampos>(line.size() + 1);
            pos = approxRead;
        }
        if (totalBytes > 0) {
            uint32_t percent = static_cast<uint32_t>((static_cast<long double>(pos) * 100.0L) / static_cast<long double>(totalBytes));
            if (percent > 100) percent = 100;
            if (percent != lastPercent) {
                lastPercent = percent;
                printProgressBar("Reading file", percent / 100.0f);
            }
        }
    }
    
    mesh->calculateBounds();
    std::cout << "\nLoaded " << mesh->vertices.size() << " vertices" << std::endl;
    
    return mesh;
}
