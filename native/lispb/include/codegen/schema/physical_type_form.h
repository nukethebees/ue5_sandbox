#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class PhysicalTypeForm : std::uint8_t {
    value,
    object_pointer,
    lvalue_reference,
    rvalue_reference,
    unsupported
};

} // namespace lispb::codegen
