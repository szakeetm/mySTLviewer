#include "XMLLoader.h"
#include <pugixml.hpp>
#include <iostream>
#include <unordered_map>

std::unique_ptr<Mesh> XMLLoader::load(const std::string& filename) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filename.c_str());
    
    if (!result) {
        std::cerr << "Failed to parse XML file: " << filename << std::endl;
        std::cerr << "Error: " << result.description() << std::endl;
        return nullptr;
    }
    
    auto mesh = std::make_unique<Mesh>();
    
    // Navigate to Geometry node
    pugi::xml_node geometry = doc.child("SimulationEnvironment").child("Geometry");
    if (!geometry) {
        std::cerr << "No Geometry node found in XML" << std::endl;
        return nullptr;
    }
    
    // Load vertices
    pugi::xml_node vertices_node = geometry.child("Vertices");
    if (!vertices_node) {
        std::cerr << "No Vertices node found in XML" << std::endl;
        return nullptr;
    }
    
    int nb_vertices = vertices_node.attribute("nb").as_int();
    std::cout << "Loading XML geometry with " << nb_vertices << " vertices..." << std::endl;
    
    // Reserve space for vertices
    mesh->vertices.reserve(nb_vertices);
    
    // Map to keep track of vertex ID to index mapping
    std::unordered_map<int, unsigned int> vertexIdToIndex;
    
    // Parse vertices
    for (pugi::xml_node vertex : vertices_node.children("Vertex")) {
        int id = vertex.attribute("id").as_int();
        float x = vertex.attribute("x").as_float();
        float y = vertex.attribute("y").as_float();
        float z = vertex.attribute("z").as_float();
        
        Vertex v;
        v.position = glm::vec3(x, y, z);
        v.normal = glm::vec3(0.0f); // Will be computed per-facet
        
        vertexIdToIndex[id] = mesh->vertices.size();
        mesh->vertices.push_back(v);
    }
    
    // Load facets
    pugi::xml_node facets_node = geometry.child("Facets");
    if (!facets_node) {
        std::cerr << "No Facets node found in XML" << std::endl;
        return nullptr;
    }
    
    int nb_facets = facets_node.attribute("nb").as_int();
    std::cout << "Loading " << nb_facets << " facets..." << std::endl;
    
    mesh->facets.reserve(nb_facets);
    
    // Parse facets
    int facetCount = 0;
    for (pugi::xml_node facet : facets_node.children("Facet")) {
        pugi::xml_node indices_node = facet.child("Indices");
        if (!indices_node) {
            continue; // Skip facets without indices
        }
        
        Facet f;
        int nb_indices = indices_node.attribute("nb").as_int();
        f.indices.reserve(nb_indices);
        
        // Collect vertex indices for this facet
        std::vector<glm::vec3> facetPositions;
        for (pugi::xml_node indice : indices_node.children("Indice")) {
            int vertexId = indice.attribute("vertex").as_int();
            
            // Map vertex ID to our internal index
            auto it = vertexIdToIndex.find(vertexId);
            if (it != vertexIdToIndex.end()) {
                unsigned int vertexIndex = it->second;
                f.indices.push_back(vertexIndex);
                facetPositions.push_back(mesh->vertices[vertexIndex].position);
            }
        }
        
        // Compute facet normal using Newell's method (robust for non-planar polygons)
        glm::vec3 normal(0.0f);
        if (facetPositions.size() >= 3) {
            for (size_t i = 0; i < facetPositions.size(); ++i) {
                const glm::vec3& v1 = facetPositions[i];
                const glm::vec3& v2 = facetPositions[(i + 1) % facetPositions.size()];
                
                normal.x += (v1.y - v2.y) * (v1.z + v2.z);
                normal.y += (v1.z - v2.z) * (v1.x + v2.x);
                normal.z += (v1.x - v2.x) * (v1.y + v2.y);
            }
            
            float length = glm::length(normal);
            if (length > 0.0001f) {
                normal = glm::normalize(normal);
            } else {
                // Fallback: use cross product of first two edges
                if (facetPositions.size() >= 3) {
                    glm::vec3 edge1 = facetPositions[1] - facetPositions[0];
                    glm::vec3 edge2 = facetPositions[2] - facetPositions[0];
                    normal = glm::normalize(glm::cross(edge1, edge2));
                }
            }
        }
        
        // Assign the computed normal to all vertices in this facet
        for (unsigned int idx : f.indices) {
            mesh->vertices[idx].normal = normal;
        }
        
        mesh->facets.push_back(f);
        facetCount++;
    }
    
    std::cout << "Loaded " << mesh->vertices.size() << " vertices and " 
              << mesh->facets.size() << " facets from XML" << std::endl;
    
    mesh->calculateBounds();
    
    return mesh;
}

bool XMLLoader::isXMLGeometry(const std::string& filename) {
    // Simple check: does it end with .xml?
    size_t dotPos = filename.find_last_of('.');
    if (dotPos != std::string::npos) {
        std::string ext = filename.substr(dotPos);
        return (ext == ".xml" || ext == ".XML");
    }
    return false;
}
