#pragma once

#include <HAL/Platform.h>
#include <Misc/AssertionMacros.h>

#include <SandboxCore/fixed_array.h>

#include <limits>

namespace ml {
enum class EBlockAllocationMode : uint8 {
    Unreal,
    VirtualAlloc2,
};

class SANDBOXCORE_API FBlockAllocator {
  public:
    explicit FBlockAllocator(EBlockAllocationMode mode) noexcept;
    ~FBlockAllocator();

    FBlockAllocator(FBlockAllocator const&) = delete;
    auto operator=(FBlockAllocator const&) -> FBlockAllocator& = delete;
    FBlockAllocator(FBlockAllocator&& other) noexcept;
    auto operator=(FBlockAllocator&& other) noexcept -> FBlockAllocator&;

    auto allocation_mode() const noexcept -> EBlockAllocationMode { return allocation_mode_; }

    template <typename T>
    auto allocate(int32 const count) -> T* {
        check(count >= 0);

        SIZE_T const object_count{static_cast<SIZE_T>(count)};
        check(object_count <= std::numeric_limits<SIZE_T>::max() / sizeof(T));

        return static_cast<T*>(allocate_impl(object_count * sizeof(T), alignof(T)));
    }
  private:
    auto allocate_impl(SIZE_T bytes, SIZE_T alignment) -> void*;
    void release_allocations();
    void free_allocation(void* data);

    struct FAllocation {
        void* data{nullptr};
        SIZE_T bytes{0};
    };

    static constexpr int32 max_allocation_count{8};

    EBlockAllocationMode allocation_mode_;
    TFixedArray<FAllocation, max_allocation_count> allocations_{};
};
}
