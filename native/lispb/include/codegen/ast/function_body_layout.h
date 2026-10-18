#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class FunctionBodyLayout : std::uint8_t {
    expanded,
    compact,
};

} // namespace lispb::codegen
