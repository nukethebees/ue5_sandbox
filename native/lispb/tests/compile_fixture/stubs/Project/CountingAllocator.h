#pragma once

#include <SandboxCore/mimalloc_storage_allocator.h>

struct CountingAllocator {
    inline static int allocations{};
    inline static int frees{};
    inline static SIZE_T last_bytes{};
    inline static uint32 last_alignment{};
    inline static bool reject_allocation{};

    static auto allocate(SIZE_T bytes, uint32 alignment) -> std::byte* {
        last_bytes = bytes;
        last_alignment = alignment;
        if (reject_allocation) {
            throw std::bad_alloc{};
        }
        auto* data{ml::soa_storage::MimallocStorageAllocator::allocate(bytes, alignment)};
        ++allocations;
        return data;
    }
    static void free(std::byte* data) noexcept {
        if (data) {
            ++frees;
        }
        ml::soa_storage::MimallocStorageAllocator::free(data);
    }
};
