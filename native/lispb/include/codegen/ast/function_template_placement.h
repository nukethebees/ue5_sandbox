#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class FunctionTemplatePlacement : std::uint8_t {
    separate_line,
    same_line,
};

} // namespace lispb::codegen
