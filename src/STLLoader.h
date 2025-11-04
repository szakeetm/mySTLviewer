#pragma once

#include "Mesh.h"
#include <string>
#include <memory>
#include <functional>

class STLLoader {
public:
    // Progress callback: (progress, message) where progress is 0.0 to 1.0
    using ProgressCallback = std::function<void(float, const std::string&)>;
    
    static std::unique_ptr<Mesh> load(const std::string& filename, ProgressCallback progressCallback = nullptr);
    
private:
    static std::unique_ptr<Mesh> loadBinary(const std::string& filename, ProgressCallback progressCallback);
    static std::unique_ptr<Mesh> loadASCII(const std::string& filename, ProgressCallback progressCallback);
    static bool isBinarySTL(const std::string& filename);
};
