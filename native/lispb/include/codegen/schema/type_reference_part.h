#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class TypeReferencePart : std::uint8_t {
    direct,
    relationship,
    enum_accessor,
    return_type,
    parameter,
    trailing_return,
    dependency
};

} // namespace lispb::codegen
