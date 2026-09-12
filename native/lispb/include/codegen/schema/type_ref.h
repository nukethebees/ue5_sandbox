#pragma once

#include <optional>
#include <string>

namespace codegen {

struct TypeRef {
    std::string name;
    std::string suffix;
    std::optional<std::string> nested;
};

} // namespace codegen
