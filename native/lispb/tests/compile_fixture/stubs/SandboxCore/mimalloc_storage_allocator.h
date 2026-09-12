#pragma once

#include <cstddef>
#include <HAL/UnrealMemory.h>
#include <new>

namespace ml::soa_storage {
// Compile fixtures use platform storage; Unreal runtime tests verify the actual DLL allocator.
struct MimallocStorageAllocator {
    static auto allocate(SIZE_T bytes, uint32 alignment) -> std::byte* {
        auto* const data{FMemory::Malloc(bytes, alignment)};
        if (data == nullptr) {
            throw std::bad_alloc{};
        }
        return ::new (data) std::byte[bytes];
    }
    static void free(std::byte* data) noexcept { FMemory::Free(data); }
};
}
