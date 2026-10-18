#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class ParameterPassing : std::uint8_t {
    const_reference,
    value,
};

} // namespace lispb::codegen
