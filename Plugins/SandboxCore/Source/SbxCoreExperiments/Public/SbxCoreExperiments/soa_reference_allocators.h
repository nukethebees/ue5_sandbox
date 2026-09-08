#pragma once

#include <Containers/ContainerAllocationPolicies.h>
#include <HAL/UnrealMemory.h>
#include <SbxCoreExperiments/mimalloc_storage_allocator.h>

#include <algorithm>

namespace ml::single_allocation_experiment {

template <bool UseRealloc>
class ReferenceAllocator : public TSizedAllocatorBase<32, ReferenceAllocator<UseRealloc>> {
    using Base = TSizedAllocatorBase<32, ReferenceAllocator<UseRealloc>>;
  public:
    class ForAnyElementType : public Base::ForAnyElementType {
      public:
        auto CallRealloc(void* const data,
                         int32 const capacity,
                         SIZE_T const element_bytes,
                         uint32 const element_alignment = DEFAULT_ALIGNMENT) -> void* {
            if (capacity == 0) {
                FMemory::Free(data);
                return nullptr;
            }
            auto const bytes{static_cast<SIZE_T>(capacity) * element_bytes};
            auto const alignment{std::max(uint32{64}, element_alignment)};
            if constexpr (UseRealloc) {
                return FMemory::Realloc(data, bytes, alignment);
            } else {
                auto* const result{FMemory::Malloc(bytes, alignment)};
                if (data != nullptr) {
                    auto const copy_bytes{std::min(FMemory::GetAllocSize(data), bytes)};
                    FMemory::Memcpy(result, data, copy_bytes);
                    FMemory::Free(data);
                }
                return result;
            }
        }

        void CallFree(void* const data) { FMemory::Free(data); }

        // Keep empty-reserve requests identical to the raw allocation references.
        auto CalculateSlackReserve(int32 const capacity, SIZE_T, uint32 = DEFAULT_ALIGNMENT) const
            -> int32 {
            return capacity;
        }
    };
};

using MallocAllocator = ReferenceAllocator<false>;
using ReallocAllocator = ReferenceAllocator<true>;

class MimallocArrayAllocator : public TSizedAllocatorBase<32, MimallocArrayAllocator> {
    using Base = TSizedAllocatorBase<32, MimallocArrayAllocator>;
  public:
    class ForAnyElementType : public Base::ForAnyElementType {
      public:
        auto CallRealloc(void* data,
                         int32 const capacity,
                         SIZE_T const element_bytes,
                         uint32 const element_alignment = DEFAULT_ALIGNMENT) -> void* {
            if (capacity == 0) {
                CallFree(data);
                return nullptr;
            }
            auto const bytes{static_cast<SIZE_T>(capacity) * element_bytes};
            return MimallocStorageAllocator::reallocate(
                data, bytes, std::max(uint32{64}, element_alignment));
        }
        void CallFree(void* data) { MimallocStorageAllocator::free(static_cast<std::byte*>(data)); }
        auto CalculateSlackReserve(int32 const capacity, SIZE_T, uint32 = DEFAULT_ALIGNMENT) const
            -> int32 {
            return capacity;
        }
    };
};

}

template <bool UseRealloc>
struct TAllocatorTraits<ml::single_allocation_experiment::ReferenceAllocator<UseRealloc>>
    : TAllocatorTraitsBase<ml::single_allocation_experiment::ReferenceAllocator<UseRealloc>> {
    enum { SupportsElementAlignment = true };
};

template <>
struct TAllocatorTraits<ml::single_allocation_experiment::MimallocArrayAllocator>
    : TAllocatorTraitsBase<ml::single_allocation_experiment::MimallocArrayAllocator> {
    enum { SupportsElementAlignment = true };
};
