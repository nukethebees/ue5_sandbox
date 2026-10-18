#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class TypeReferencePart : std::uint8_t {
    direct,
    relationship,
    return_type,
    parameter,
    trailing_return,
    equivalent_type,
    input,
    dependency
};

} // namespace lispb::codegen
