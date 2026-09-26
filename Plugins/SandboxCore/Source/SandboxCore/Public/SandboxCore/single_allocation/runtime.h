#pragma once

#include <sandbox/core/single_allocation/layout.h>
#include <sandbox/core/single_allocation/view.h>

#include <Containers/ArrayView.h>
#include <Containers/ContainerAllocationPolicies.h>
#include <HAL/UnrealMemory.h>
#include <Misc/AssertionMacros.h>
#include <Templates/MemoryOps.h>

#include <cstddef>
#include <cstdlib>

namespace ml::soa_storage {

using single_allocation_layout::capacity_block_bound;
using single_allocation_layout::capacity_granularity;
using single_allocation_layout::ColumnLayout;
using single_allocation_layout::ColumnLayoutStart;
using single_allocation_layout::layout_align;
using single_allocation_layout::LayoutCursor;
using single_allocation_layout::LayoutPolicy;
using single_allocation_layout::maximum_capacity;
using single_allocation_layout::supported_leaf;
using single_allocation_layout::try_allocation_bytes;
using single_allocation_layout::try_round_capacity;
using soa_storage_detail::source_data;

[[noreturn]] inline void invalid_size() {
    LowLevelFatalError(TEXT("Single-allocation SoA: invalid size, range, or allocation overflow."));
    std::abort();
}

inline void require(bool const condition) {
    if (!condition) {
        invalid_size();
    }
}

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

using soa_storage_detail::StorageState;
template <bool Const>
using CompactViewState = soa_storage_detail::CompactViewState<Const, require>;

inline auto rounded_capacity(int64 const required, SIZE_T const block_bytes) -> int32 {
    int32 result{};
    require(try_round_capacity(required, maximum_capacity(block_bytes), result));
    return result;
}

inline auto growth_capacity(int32 const required, int32 const current, SIZE_T const block_bytes)
    -> int32 {
    auto const maximum{maximum_capacity(block_bytes)};
    require(required > current && required <= maximum);
    auto const geometric{DefaultCalculateSlackGrow<SIZE_T>(
        static_cast<SIZE_T>(required), static_cast<SIZE_T>(current), 1, false)};
    auto const bounded{geometric < static_cast<SIZE_T>(maximum) ? geometric
                                                                : static_cast<SIZE_T>(maximum)};
    return rounded_capacity(static_cast<int64>(bounded), block_bytes);
}

inline auto allocation_bytes(int32 const capacity, SIZE_T const block_bytes) -> SIZE_T {
    SIZE_T result{};
    require(try_allocation_bytes(capacity, block_bytes, result));
    return result;
}

}
