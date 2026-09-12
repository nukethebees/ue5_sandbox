#pragma once

#include <optional>
#include <string>

namespace codegen {

struct Include {
    std::string path;
    std::optional<bool> system;
};

} // namespace codegen
