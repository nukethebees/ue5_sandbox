#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class StorageOperation : std::uint8_t {
    reset,
    reserve,
    add_uninitialised,
    add_defaulted,
    remove_at_swap,
    set_num,
    copy_element,
    append_from,
};

} // namespace lispb::codegen
