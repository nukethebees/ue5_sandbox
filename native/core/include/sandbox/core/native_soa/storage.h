#pragma once

#include <sandbox/core/address_cast.h>
#include <sandbox/core/compact_vector_view.h>
#include <sandbox/core/single_allocation/layout.h>
#include <sandbox/core/single_allocation/operations.h>
#include <sandbox/core/single_allocation/removal.h>
#include <sandbox/core/single_allocation/view.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef NATIVE_SOA_MIMALLOC
#define NATIVE_SOA_MIMALLOC 0
#endif

#if NATIVE_SOA_MIMALLOC
#include <sbx/memory.h>
#endif

namespace ml::native_soa {

#if NATIVE_SOA_MIMALLOC
template <typename T>
struct MimallocAllocator {
    using value_type = T;
    MimallocAllocator() = default;
    template <typename U>
    MimallocAllocator(MimallocAllocator<U> const&) noexcept {}
    auto allocate(std::size_t count) -> T* {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length{};
        }
        auto* const allocation{sbx::memory::allocate_aligned(count * sizeof(T), alignof(T))};
        if (allocation == nullptr) {
            throw std::bad_alloc{};
        }
        return static_cast<T*>(allocation);
    }
    void deallocate(T* data, std::size_t) noexcept { sbx::memory::free(data); }
    friend auto operator==(MimallocAllocator const&, MimallocAllocator const&) -> bool = default;
};
template <typename T>
using Vector = std::vector<T, MimallocAllocator<T>>;
#else
template <typename T>
using Vector = std::vector<T>;
#endif

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

template <typename Destination, typename Source>
auto is_external_source(Destination const& destination, Source const& source) -> bool {
    auto const address{ml::address_cast(source.data())};
    auto const begin{ml::address_cast(destination.data())};
    return address < begin || address >= begin + destination.size() * sizeof(*destination.data());
}

template <typename T>
void copy_n(T* const destination, T const* const source, std::uint32_t const count) noexcept {
    if (count == 0) {
        return;
    }
    std::memcpy(destination, source, static_cast<std::size_t>(count) * sizeof(T));
}

template <typename T>
void move_n(T* const destination, T const* const source, std::uint32_t const count) noexcept {
    if (count == 0) {
        return;
    }
    std::memmove(destination, source, static_cast<std::size_t>(count) * sizeof(T));
}

template <typename T>
void default_construct_n(T* const destination, std::uint32_t const count) {
    std::uninitialized_value_construct_n(destination, count);
}

using StorageState = soa_storage_detail::StorageState<std::uint32_t>;

inline auto rounded_capacity(std::int64_t const required,
                             [[maybe_unused]] std::size_t const block_bytes) -> std::uint32_t {
    assert(required >= 0 && required <= maximum_capacity(block_bytes));
    auto const rounded{((required + capacity_granularity - 1) / capacity_granularity) *
                       capacity_granularity};
    assert(rounded <= maximum_capacity(block_bytes));
    return static_cast<std::uint32_t>(rounded);
}

inline auto growth_capacity(std::uint32_t const required,
                            std::uint32_t const current,
                            std::size_t const block_bytes) -> std::uint32_t {
    auto const maximum{maximum_capacity(block_bytes)};
    assert(required > current && required <= maximum);
    auto const geometric{
        std::max(static_cast<std::size_t>(required),
                 static_cast<std::size_t>(current) + static_cast<std::size_t>(current) / 2)};
    auto const bounded{geometric < static_cast<std::size_t>(maximum)
                           ? geometric
                           : static_cast<std::size_t>(maximum)};
    return rounded_capacity(static_cast<std::int64_t>(bounded), block_bytes);
}

inline auto allocation_bytes(std::uint32_t const capacity, std::size_t const block_bytes)
    -> std::size_t {
    assert(capacity >= 0 && capacity % capacity_granularity == 0 &&
           capacity <= maximum_capacity(block_bytes));
    return static_cast<std::size_t>(capacity / capacity_granularity) * block_bytes;
}

using StorageOperations =
    soa_storage_detail::StorageOperations<std::uint32_t, rounded_capacity, growth_capacity>;

}

namespace ml::native_soa {
template <typename T>
using Vector2View = soa_storage_detail::VectorView<T, 2, std::span>;
template <typename T>
using Vector2ConstView = Vector2View<T const>;
template <typename T>
using Vector3View = soa_storage_detail::VectorView<T, 3, std::span>;
template <typename T>
using Vector3ConstView = Vector3View<T const>;
}
