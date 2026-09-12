#pragma once

#include <optional>
#include <string>

namespace codegen {

struct Enumerator {
    std::string name;
    std::optional<std::string> initializer;
    std::optional<std::string> annotation;
};

} // namespace codegen
