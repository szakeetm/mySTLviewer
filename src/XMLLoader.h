#pragma once

#include "Mesh.h"
#include <string>
#include <memory>

class XMLLoader {
public:
    static std::unique_ptr<Mesh> load(const std::string& filename);
    
private:
    static bool isXMLGeometry(const std::string& filename);
};
