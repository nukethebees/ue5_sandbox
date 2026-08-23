#pragma once

#include <SandboxCore/block_allocator.h>

namespace ml::detail {
auto allocate_virtual_alloc2(SIZE_T bytes, SIZE_T alignment, EVirtualAlloc2PageMode page_mode)
    -> void*;
void free_virtual_alloc2(void* data);
}
