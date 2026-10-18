#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class TypeOperation : std::uint8_t {
    add_element,
    remove_at_swap,
    set_element,
};

} // namespace lispb::codegen
