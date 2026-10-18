#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class FunctionTrailingReturnPlacement : std::uint8_t {
    same_line,
    next_line,
};

} // namespace lispb::codegen
