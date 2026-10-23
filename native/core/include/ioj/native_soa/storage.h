#pragma once

#include <ioj/single_allocation/layout.h>
#include <ioj/single_allocation/operations.h>
#include <ioj/single_allocation/view.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace ioj::native_soa {

using size_type = std::uint32_t;
using byte_size_type = std::size_t;

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

template <bool Overlapping, typename T>
void transfer_n(T* const destination, T const* const source, size_type const count) noexcept {
    if (count == 0) {
        return;
    }
    auto const bytes{static_cast<byte_size_type>(count) * sizeof(T)};
    if constexpr (Overlapping) {
        std::memmove(destination, source, bytes);
    } else {
        std::memcpy(destination, source, bytes);
    }
}

template <typename T>
void default_construct_n(T* const destination, size_type const count) {
    std::uninitialized_value_construct_n(destination, count);
}

using StorageState = soa_storage_detail::StorageState;

inline auto rounded_capacity(size_type const required,
                             [[maybe_unused]] byte_size_type const block_bytes) -> size_type {
    assert(required <= maximum_capacity(block_bytes));
    auto const rounded{
        ((byte_size_type{required} + capacity_granularity - 1) / capacity_granularity) *
        capacity_granularity};
    assert(rounded <= maximum_capacity(block_bytes));
    return static_cast<size_type>(rounded);
}

inline auto growth_capacity(size_type const required,
                            size_type const current,
                            byte_size_type const block_bytes) -> size_type {
    auto const maximum{maximum_capacity(block_bytes)};
    assert(required > current && required <= maximum);
    auto const geometric{
        std::max(static_cast<byte_size_type>(required),
                 static_cast<byte_size_type>(current) + static_cast<byte_size_type>(current) / 2)};
    auto const bounded{geometric < static_cast<byte_size_type>(maximum)
                           ? geometric
                           : static_cast<byte_size_type>(maximum)};
    return rounded_capacity(static_cast<size_type>(bounded), block_bytes);
}

using StorageOperations = soa_storage_detail::StorageOperations<rounded_capacity, growth_capacity>;

}
