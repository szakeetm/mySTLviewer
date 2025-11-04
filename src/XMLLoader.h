#pragma once

#include "Mesh.h"
#include <string>
#include <memory>

// Forward declaration - can be Progress_abstract (global) or gui::Progress_abstract
class Progress_abstract;

// Forward declaration
namespace pugi {
    class xml_document;
}

class XMLLoader {
public:
    // Load XML/zip file with optional progress reporting
    // progress can be nullptr if no progress reporting is needed
    // Accepts both Progress_abstract (global namespace) and gui::Progress_abstract
    static std::unique_ptr<Mesh> load(const std::string& filename, Progress_abstract* progress = nullptr);
    
private:
    static bool isXMLGeometry(const std::string& filename);
    static bool isZipFile(const std::string& filename);
    static std::unique_ptr<Mesh> loadFromZip(const std::string& filename, Progress_abstract* progress);
    static std::unique_ptr<Mesh> loadFromXMLString(const pugi::xml_document& doc, Progress_abstract* progress);
};
