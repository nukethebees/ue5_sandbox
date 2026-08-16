#include <SandboxCore/block_allocator.h>

#include <HAL/UnrealMemory.h>
#include <Misc/AssertionMacros.h>

#include <limits>

namespace ml {
FBlockAllocator::~FBlockAllocator() {
    for (FAllocation const& allocation : allocations_) {
        FMemory::Free(allocation.data);
    }
}

auto FBlockAllocator::allocate_impl(SIZE_T const bytes, SIZE_T const alignment) -> void* {
    check(!allocations_.is_full());
    check(alignment <= std::numeric_limits<uint32>::max());

    void* const data{FMemory::Malloc(bytes, static_cast<uint32>(alignment))};
    check(data != nullptr);

    allocations_.emplace_back(FAllocation{.data = data, .bytes = bytes});
    return data;
}
}
