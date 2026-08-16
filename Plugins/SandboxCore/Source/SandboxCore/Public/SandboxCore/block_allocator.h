#pragma once

#include <HAL/Platform.h>
#include <Misc/AssertionMacros.h>

#include <SandboxCore/fixed_array.h>

#include <limits>

namespace ml {
class SANDBOXCORE_API FBlockAllocator {
  public:
    FBlockAllocator() = default;
    ~FBlockAllocator();

    FBlockAllocator(FBlockAllocator const&) = delete;
    auto operator=(FBlockAllocator const&) -> FBlockAllocator& = delete;
    FBlockAllocator(FBlockAllocator&&) = delete;
    auto operator=(FBlockAllocator&&) -> FBlockAllocator& = delete;

    template <typename T>
    auto allocate(int32 const count) -> T* {
        check(count >= 0);

        SIZE_T const object_count{static_cast<SIZE_T>(count)};
        check(object_count <= std::numeric_limits<SIZE_T>::max() / sizeof(T));

        return static_cast<T*>(allocate_impl(object_count * sizeof(T), alignof(T)));
    }

  private:
    auto allocate_impl(SIZE_T bytes, SIZE_T alignment) -> void*;

    struct FAllocation {
        void* data{nullptr};
        SIZE_T bytes{0};
    };

    static constexpr int32 max_allocation_count{8};

    TFixedArray<FAllocation, max_allocation_count> allocations_{};
};
}
