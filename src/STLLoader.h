#pragma once

#include "Mesh.h"
#include <string>
#include <memory>

class STLLoader {
public:
    static std::unique_ptr<Mesh> load(const std::string& filename);
    
private:
    static std::unique_ptr<Mesh> loadBinary(const std::string& filename);
    static std::unique_ptr<Mesh> loadASCII(const std::string& filename);
    static bool isBinarySTL(const std::string& filename);
};
