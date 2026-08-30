#pragma once

#include <optional>
#include <string>

namespace codegen {

struct EnumeratorSchema {
    std::string name;
    std::optional<std::string> initializer;
    std::optional<std::string> display_name;
    bool hidden{false};
};

} // namespace codegen
