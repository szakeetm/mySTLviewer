#include "XMLLoader.h"
#include <pugixml.hpp>
#include <archive.h>
#include <archive_entry.h>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <cstring>

std::unique_ptr<Mesh> XMLLoader::load(const std::string& filename) {
    // Check if it's a zip file
    if (isZipFile(filename)) {
        return loadFromZip(filename);
    }
    
    // Otherwise load as regular XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filename.c_str());
    
    if (!result) {
        std::cerr << "Failed to parse XML file: " << filename << std::endl;
        std::cerr << "Error: " << result.description() << std::endl;
        return nullptr;
    }
    
    return loadFromXMLString(doc);
}

std::unique_ptr<Mesh> XMLLoader::loadFromXMLString(const pugi::xml_document& doc) {
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

bool XMLLoader::isZipFile(const std::string& filename) {
    size_t dotPos = filename.find_last_of('.');
    if (dotPos != std::string::npos) {
        std::string ext = filename.substr(dotPos);
        return (ext == ".zip" || ext == ".ZIP");
    }
    return false;
}

std::unique_ptr<Mesh> XMLLoader::loadFromZip(const std::string& filename) {
    std::cout << "Loading XML from zip archive: " << filename << std::endl;
    
    struct archive* a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    
    int r = archive_read_open_filename(a, filename.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        std::cerr << "Failed to open zip archive: " << archive_error_string(a) << std::endl;
        archive_read_free(a);
        return nullptr;
    }
    
    std::unique_ptr<Mesh> mesh = nullptr;
    struct archive_entry* entry;
    bool foundXML = false;
    
    // Iterate through entries in the archive
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char* entryName = archive_entry_pathname(entry);
        std::string name(entryName);
        
        // Check if this is an XML file
        size_t dotPos = name.find_last_of('.');
        if (dotPos != std::string::npos) {
            std::string ext = name.substr(dotPos);
            if (ext == ".xml" || ext == ".XML") {
                if (foundXML) {
                    std::cerr << "Warning: Multiple XML files found in zip. Using first one." << std::endl;
                    archive_read_data_skip(a);
                    continue;
                }
                
                foundXML = true;
                std::cout << "Found XML file in archive: " << name << std::endl;
                
                // Read the entire file content
                size_t size = archive_entry_size(entry);
                std::vector<char> buffer(size + 1);
                
                la_ssize_t bytes_read = archive_read_data(a, buffer.data(), size);
                if (bytes_read < 0) {
                    std::cerr << "Error reading from archive: " << archive_error_string(a) << std::endl;
                    archive_read_free(a);
                    return nullptr;
                }
                
                buffer[bytes_read] = '\0';
                
                // Parse the XML from the buffer
                pugi::xml_document doc;
                pugi::xml_parse_result result = doc.load_buffer(buffer.data(), bytes_read);
                
                if (!result) {
                    std::cerr << "Failed to parse XML from zip: " << result.description() << std::endl;
                    archive_read_free(a);
                    return nullptr;
                }
                
                mesh = loadFromXMLString(doc);
                continue;
            }
        }
        
        // Skip non-XML files
        archive_read_data_skip(a);
    }
    
    archive_read_free(a);
    
    if (!foundXML) {
        std::cerr << "No XML file found in zip archive" << std::endl;
        return nullptr;
    }
    
    return mesh;
}
