#pragma once

#include <cstddef>
#include <string>
namespace material_synth {

struct Diagnostic {
    std::string path;
    std::size_t line{1};
    std::size_t column{1};
    std::string message;
};

}
