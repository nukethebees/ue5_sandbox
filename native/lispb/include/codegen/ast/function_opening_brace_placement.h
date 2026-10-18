#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class FunctionOpeningBracePlacement : std::uint8_t {
    same_line,
    separate_line,
};

} // namespace lispb::codegen
