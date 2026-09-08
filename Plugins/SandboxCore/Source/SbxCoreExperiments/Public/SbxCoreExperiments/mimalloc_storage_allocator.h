#pragma once

#include <CoreTypes.h>
#include <cstddef>

namespace ml::single_allocation_experiment {
struct SBXCOREEXPERIMENTS_API MimallocStorageAllocator {
    static auto allocate(SIZE_T bytes, uint32 alignment) -> std::byte*;
    static auto reallocate(void* data, SIZE_T bytes, uint32 alignment) -> void*;
    static void free(std::byte* data) noexcept;
    static auto owns(void const* data) noexcept -> bool;
};
}
