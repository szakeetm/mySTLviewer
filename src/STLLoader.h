#pragma once

#include "Mesh.h"
#include <string>
#include <memory>

// Forward declaration - can be Progress_abstract (global) or gui::Progress_abstract
class Progress_abstract;

class STLLoader {
public:
    // Load STL file with optional progress reporting
    // progress can be nullptr if no progress reporting is needed
    // Accepts both Progress_abstract (global namespace) and gui::Progress_abstract
    static std::unique_ptr<Mesh> load(const std::string& filename, Progress_abstract* progress = nullptr);
    
private:
    static std::unique_ptr<Mesh> loadBinary(const std::string& filename, Progress_abstract* progress);
    static std::unique_ptr<Mesh> loadASCII(const std::string& filename, Progress_abstract* progress);
    static bool isBinarySTL(const std::string& filename);
};
