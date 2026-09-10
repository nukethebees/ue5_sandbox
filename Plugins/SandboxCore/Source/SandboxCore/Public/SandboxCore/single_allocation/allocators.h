#pragma once

#include <SandboxCore/single_allocation/runtime.h>

#include <HAL/UnrealMemory.h>

#include <cstddef>
#include <new>

namespace ml::soa_storage {

struct FMemoryStorageAllocator {
    static auto allocate(SIZE_T const bytes, uint32 const alignment) -> std::byte* {
        auto* const allocation{FMemory::Realloc(nullptr, bytes, alignment)};
        require(allocation != nullptr);
        // The byte array provides storage and starts implicit-lifetime leaf arrays without
        // initialization.
        return ::new (allocation) std::byte[bytes];
    }
    static void free(std::byte* data) noexcept { FMemory::Free(data); }
};

}
