#pragma once

#include "Mesh.h"
#include <string>
#include <memory>

namespace gui {
    class Progress_abstract;
}

class STLLoader {
public:
    static std::unique_ptr<Mesh> load(const std::string& filename, gui::Progress_abstract* progress = nullptr);
    
private:
    static std::unique_ptr<Mesh> loadBinary(const std::string& filename, gui::Progress_abstract* progress = nullptr);
    static std::unique_ptr<Mesh> loadASCII(const std::string& filename, gui::Progress_abstract* progress = nullptr);
    static bool isBinarySTL(const std::string& filename);
};
