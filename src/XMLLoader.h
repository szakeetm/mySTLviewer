#pragma once

#include "Mesh.h"
#include <string>
#include <memory>

// Forward declaration
namespace pugi {
    class xml_document;
}

class XMLLoader {
public:
    static std::unique_ptr<Mesh> load(const std::string& filename);
    
private:
    static bool isXMLGeometry(const std::string& filename);
    static bool isZipFile(const std::string& filename);
    static std::unique_ptr<Mesh> loadFromZip(const std::string& filename);
    static std::unique_ptr<Mesh> loadFromXMLString(const pugi::xml_document& doc);
};
