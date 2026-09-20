#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace codegen {

struct EnumeratorSchema {
    std::string name;
    std::optional<std::uint64_t> initializer;
    std::optional<std::string> display_name;
    bool hidden{false};
    std::optional<std::string> serialized_name;
};

} // namespace codegen
