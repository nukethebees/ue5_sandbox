#pragma once

namespace codegen {

enum class StorageOperation {
    reset,
    reserve,
    add_uninitialised,
    add_defaulted,
    remove_at_swap,
    set_num,
    copy_element,
    append_from,
};

} // namespace codegen
