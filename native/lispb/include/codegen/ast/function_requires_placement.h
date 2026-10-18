#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class FunctionRequiresPlacement : std::uint8_t {
    trailing_next_line,
    trailing_same_line,
    before_signature,
};

} // namespace lispb::codegen
