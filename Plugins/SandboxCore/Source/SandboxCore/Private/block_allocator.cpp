#include <SandboxCore/block_allocator.h>
#include <SandboxCore/log_categories.h>

#include <HAL/UnrealMemory.h>
#include <Misc/AssertionMacros.h>
#include <Templates/UnrealTemplate.h>

#include <limits>

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"

#include <memoryapi.h>
#include <windows.h>

#include "Windows/HideWindowsPlatformTypes.h"

extern "C" {
    PVOID
    WINAPI
    VirtualAlloc2(_In_opt_ HANDLE Process,
                  _In_opt_ PVOID BaseAddress,
                  _In_ SIZE_T Size,
                  _In_ ULONG AllocationType,
                  _In_ ULONG PageProtection,
                  _Inout_updates_opt_(ParameterCount) MEM_EXTENDED_PARAMETER* ExtendedParameters,
                  _In_ ULONG ParameterCount);
}
#endif

namespace ml {
namespace {
#if PLATFORM_WINDOWS
auto allocate_virtual_alloc2(SIZE_T const bytes, SIZE_T const alignment) -> void* {
    constexpr HANDLE process{nullptr};     // Use current process
    constexpr PVOID base_address{nullptr}; // Let fn choose
    constexpr ULONG allocation_type{MEM_RESERVE | MEM_COMMIT};
    constexpr ULONG page_protection{PAGE_READWRITE};

    MEM_EXTENDED_PARAMETER* extended_parameters{nullptr};
    ULONG n_extended_parameters{0};

    return VirtualAlloc2(process,
                         base_address,
                         bytes,
                         allocation_type,
                         page_protection,
                         extended_parameters,
                         n_extended_parameters);
}
#endif
}

FBlockAllocator::FBlockAllocator(EBlockAllocationMode const mode) noexcept
    : allocation_mode_{mode} {}

FBlockAllocator::~FBlockAllocator() {
    release_allocations();
}

FBlockAllocator::FBlockAllocator(FBlockAllocator&& other) noexcept
    : allocation_mode_{other.allocation_mode_}
    , allocations_{MoveTemp(other.allocations_)} {}

auto FBlockAllocator::operator=(FBlockAllocator&& other) noexcept -> FBlockAllocator& {
    if (this != &other) {
        release_allocations();
        allocation_mode_ = other.allocation_mode_;
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
            data = allocate_virtual_alloc2(bytes, alignment);

            if (!data) {
                DWORD const error{GetLastError()};
                UE_LOG(LogSandboxCore, Error, TEXT("VirtualAlloc2 failed: %lu"), error);
                checkf(false, TEXT("VirtualAlloc2 failed: %lu"), error);
            }

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
            check(VirtualFree(data, 0, MEM_RELEASE) != 0);
#else
            checkf(false, TEXT("VirtualAlloc2 allocation mode is only supported on Windows."));
#endif
            break;

        default:
            checkNoEntry();
    }
}
}
