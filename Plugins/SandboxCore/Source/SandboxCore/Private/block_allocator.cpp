#include <SandboxCore/block_allocator.h>

#include <HAL/UnrealMemory.h>
#include <Misc/AssertionMacros.h>
#include <Templates/UnrealTemplate.h>

#include <limits>

#if PLATFORM_WINDOWS
#include "Windows/block_allocator_windows.h"
#endif

namespace ml {
FBlockAllocator::FBlockAllocator(EBlockAllocationMode const mode,
                                 EVirtualAlloc2PageMode const virtual_alloc2_page_mode) noexcept
    : allocation_mode_{mode}
    , virtual_alloc2_page_mode_{virtual_alloc2_page_mode} {
    checkf(allocation_mode_ == EBlockAllocationMode::VirtualAlloc2 ||
               virtual_alloc2_page_mode_ == EVirtualAlloc2PageMode::Default,
           TEXT("VirtualAlloc2 page modes require the VirtualAlloc2 allocation mode."));
}

FBlockAllocator::~FBlockAllocator() {
    release_allocations();
}

FBlockAllocator::FBlockAllocator(FBlockAllocator&& other) noexcept
    : allocation_mode_{other.allocation_mode_}
    , virtual_alloc2_page_mode_{other.virtual_alloc2_page_mode_}
    , allocations_{MoveTemp(other.allocations_)} {}

auto FBlockAllocator::operator=(FBlockAllocator&& other) noexcept -> FBlockAllocator& {
    if (this != &other) {
        release_allocations();
        allocation_mode_ = other.allocation_mode_;
        virtual_alloc2_page_mode_ = other.virtual_alloc2_page_mode_;
        allocations_ = MoveTemp(other.allocations_);
    }

    return *this;
}

auto FBlockAllocator::allocate_impl(SIZE_T const bytes, SIZE_T const alignment) -> void* {
    check(!allocations_.is_full());

    void* data{nullptr};

    switch (allocation_mode_) {
        case EBlockAllocationMode::Unreal:
            check(alignment <= std::numeric_limits<uint32>::max());
            data = FMemory::Malloc(bytes, static_cast<uint32>(alignment));
            break;

        case EBlockAllocationMode::VirtualAlloc2:
#if PLATFORM_WINDOWS
            data = detail::allocate_virtual_alloc2(bytes, alignment, virtual_alloc2_page_mode_);
#else
            checkf(false, TEXT("VirtualAlloc2 allocation mode is only supported on Windows."));
#endif
            break;

        default:
            checkNoEntry();
    }

    check(data != nullptr);

    allocations_.emplace_back(FAllocation{.data = data, .bytes = bytes});
    return data;
}

void FBlockAllocator::release_allocations() {
    for (FAllocation const& allocation : allocations_) {
        free_allocation(allocation.data);
    }

    allocations_.reset();
}

void FBlockAllocator::free_allocation(void* const data) {
    switch (allocation_mode_) {
        case EBlockAllocationMode::Unreal:
            FMemory::Free(data);
            break;

        case EBlockAllocationMode::VirtualAlloc2:
#if PLATFORM_WINDOWS
            detail::free_virtual_alloc2(data);
#else
            checkf(false, TEXT("VirtualAlloc2 allocation mode is only supported on Windows."));
#endif
            break;

        default:
            checkNoEntry();
    }
}
}
