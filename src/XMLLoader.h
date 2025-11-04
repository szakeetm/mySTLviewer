#pragma once

#include "Mesh.h"
#include <string>
#include <memory>
#include <functional>

// Forward declaration
namespace pugi {
    class xml_document;
}

class XMLLoader {
public:
    // Progress callback: (progress, message) where progress is 0.0 to 1.0
    using ProgressCallback = std::function<void(float, const std::string&)>;
    
    static std::unique_ptr<Mesh> load(const std::string& filename, ProgressCallback progressCallback = nullptr);
    
private:
    static bool isXMLGeometry(const std::string& filename);
    static bool isZipFile(const std::string& filename);
    static std::unique_ptr<Mesh> loadFromZip(const std::string& filename, XMLLoader::ProgressCallback progressCallback);
    static std::unique_ptr<Mesh> loadFromXMLString(const pugi::xml_document& doc, XMLLoader::ProgressCallback progressCallback);
};
