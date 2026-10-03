#pragma once

#include <sandbox/core/single_allocation/layout.h>
#include <sandbox/core/single_allocation/view.h>

#include <Containers/ArrayView.h>
#include <Containers/ContainerAllocationPolicies.h>
#include <HAL/UnrealMemory.h>
#include <Templates/MemoryOps.h>

#include <cassert>
#include <cstddef>

namespace ml::soa_storage {

using single_allocation_layout::capacity_block_bound;
using single_allocation_layout::capacity_granularity;
using single_allocation_layout::ColumnLayout;
using single_allocation_layout::ColumnLayoutStart;
using single_allocation_layout::layout_align;
using single_allocation_layout::LayoutCursor;
using single_allocation_layout::LayoutPolicy;
constexpr auto maximum_capacity(std::size_t const block_bytes) noexcept -> int32 {
    return single_allocation_layout::maximum_capacity<int32>(block_bytes);
}
using single_allocation_layout::supported_leaf;
using single_allocation_layout::try_allocation_bytes;
using single_allocation_layout::try_round_capacity;
using soa_storage_detail::source_data;

template <typename T>
void copy_n(T* const destination, T const* const source, int32 const count) noexcept {
    if (count == 0) {
        return;
    }
    FMemory::Memcpy(destination, source, static_cast<SIZE_T>(count) * sizeof(T));
}

template <typename T>
void move_n(T* const destination, T const* const source, int32 const count) noexcept {
    if (count == 0) {
        return;
    }
    FMemory::Memmove(destination, source, static_cast<SIZE_T>(count) * sizeof(T));
}

template <typename T>
void default_construct_n(T* const destination, int32 const count) {
    DefaultConstructItems<T>(destination, count);
}

using StorageState = soa_storage_detail::StorageState<int32>;

inline auto rounded_capacity(int64 const required, [[maybe_unused]] SIZE_T const block_bytes)
    -> int32 {
    assert(required >= 0 && required <= maximum_capacity(block_bytes));
    auto const rounded{((required + capacity_granularity - 1) / capacity_granularity) *
                       capacity_granularity};
    assert(rounded <= maximum_capacity(block_bytes));
    return static_cast<int32>(rounded);
}

inline auto growth_capacity(int32 const required, int32 const current, SIZE_T const block_bytes)
    -> int32 {
    auto const maximum{maximum_capacity(block_bytes)};
    assert(required > current && required <= maximum);
    auto const geometric{DefaultCalculateSlackGrow<SIZE_T>(
        static_cast<SIZE_T>(required), static_cast<SIZE_T>(current), 1, false)};
    auto const bounded{geometric < static_cast<SIZE_T>(maximum) ? geometric
                                                                : static_cast<SIZE_T>(maximum)};
    return rounded_capacity(static_cast<int64>(bounded), block_bytes);
}

inline auto allocation_bytes(int32 const capacity, SIZE_T const block_bytes) -> SIZE_T {
    assert(capacity >= 0 && capacity % capacity_granularity == 0 &&
           capacity <= maximum_capacity(block_bytes));
    return static_cast<SIZE_T>(capacity / capacity_granularity) * block_bytes;
}

}
