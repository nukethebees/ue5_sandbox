#pragma once

#include <sandbox/core/compact_vector_view.h>
#include <sandbox/core/single_allocation/layout.h>
#include <sandbox/core/single_allocation/removal.h>
#include <sandbox/core/single_allocation/view.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#if NATIVE_SOA_MIMALLOC
#include <mimalloc.h>
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
        return static_cast<T*>(mi_new_aligned(count * sizeof(T), alignof(T)));
    }
    void deallocate(T* data, std::size_t) noexcept { mi_free(data); }
    friend auto operator==(MimallocAllocator const&, MimallocAllocator const&) -> bool = default;
};
template <typename T>
using Vector = std::vector<T, MimallocAllocator<T>>;
#else
template <typename T>
using Vector = std::vector<T>;
#endif

using single_allocation_layout::capacity_granularity;
using single_allocation_layout::ColumnLayout;
using single_allocation_layout::ColumnLayoutStart;
using single_allocation_layout::layout_align;
using single_allocation_layout::maximum_alignment;
using single_allocation_layout::maximum_capacity;
using single_allocation_layout::supported_leaf;
using single_allocation_layout::try_allocation_bytes;
using single_allocation_layout::try_round_capacity;
[[noreturn]] inline void invalid_size() {
    std::fputs("Native SoA: invalid size, range or allocation overflow.\n", stderr);
    std::abort();
}

inline void require(bool const condition) {
    if (!condition) {
        invalid_size();
    }
}

using soa_storage_detail::StorageState;
template <bool Const>
using CompactViewState = soa_storage_detail::CompactViewState<Const, require>;

inline auto rounded_capacity(std::int64_t const required, std::size_t const block_bytes)
    -> std::int32_t {
    std::int32_t result{};
    require(try_round_capacity(required, maximum_capacity(block_bytes), result));
    return result;
}

inline auto growth_capacity(std::int32_t const required,
                            std::int32_t const current,
                            std::size_t const block_bytes) -> std::int32_t {
    auto const maximum{maximum_capacity(block_bytes)};
    require(required > current && required <= maximum);
    auto const geometric{
        std::max(static_cast<std::size_t>(required),
                 static_cast<std::size_t>(current) + static_cast<std::size_t>(current) / 2)};
    auto const bounded{geometric < static_cast<std::size_t>(maximum)
                           ? geometric
                           : static_cast<std::size_t>(maximum)};
    return rounded_capacity(static_cast<std::int64_t>(bounded), block_bytes);
}

inline auto allocation_bytes(std::int32_t const capacity, std::size_t const block_bytes)
    -> std::size_t {
    std::size_t result{};
    require(try_allocation_bytes(capacity, block_bytes, result));
    return result;
}

inline auto allocate(std::size_t const bytes, std::uint32_t const alignment) -> std::byte* {
#if NATIVE_SOA_MIMALLOC
    auto* const allocation{mi_new_aligned(bytes, alignment)};
#else
    auto* const allocation{::operator new(bytes, std::align_val_t{alignment})};
#endif
    // The byte array provides storage and starts implicit-lifetime leaf arrays without
    // initialization.
    return ::new (allocation) std::byte[bytes];
}

inline void free(std::byte* data, std::size_t alignment) noexcept {
#if NATIVE_SOA_MIMALLOC
    mi_free_aligned(data, alignment);
#else
    ::operator delete(data, std::align_val_t{alignment});
#endif
}

// Generated storage supplies the state and typed column operations.
struct StorageOperations {
    template <typename Self>
    auto num(this Self const& self) noexcept -> std::int32_t {
        return self.num_;
    }
    template <typename Self>
    auto capacity(this Self const& self) noexcept -> std::int32_t {
        return self.capacity_;
    }
    template <typename Self>
    auto is_empty(this Self const& self) noexcept -> bool {
        return self.num_ == 0;
    }
    template <typename Self>
    auto allocated_bytes(this Self const& self) -> std::size_t {
        return Self::layout_bytes(
            static_cast<std::size_t>(self.capacity_ / Self::capacity_granularity));
    }
    template <typename Self>
    void reserve(this Self& self, std::int32_t const count) {
        auto const requested{rounded_capacity(count, Self::capacity_block_bound)};
        if (requested > self.capacity_) {
            self.reallocate(requested);
        }
    }
    template <typename Self>
    void reset(this Self& self) noexcept {
        self.num_ = 0;
    }
    template <typename Self>
    void add_uninitialised(this Self& self, std::int32_t const count) {
        require(count >= 0 && count <= Self::max_capacity - self.num_);
        auto const new_num{self.num_ + count};
        if (new_num > self.capacity_) {
            self.reallocate(growth_capacity(new_num, self.capacity_, Self::capacity_block_bound));
        }
        self.num_ = new_num;
    }
    template <typename Self, typename Source>
    auto append_from(this Self& self, Source const& source) -> std::int32_t {
        typename Self::ConstView view{source.get_const_view()};
        view.validate();
        auto const count{view.num()};
        auto const first{self.num_};
        require(count <= Self::max_capacity - first);
        if (count == 0) {
            return first;
        }
        auto const new_num{first + count};
        if (new_num > self.capacity_) {
            self.reallocate(growth_capacity(new_num, self.capacity_, Self::capacity_block_bound));
        }
        self.append_columns(view.columns(), first, count);
        self.num_ = new_num;
        return first;
    }
    template <typename Self>
    void remove_at_swap(this Self& self, std::span<std::int32_t const> indices) {
        self.swap_remove_indices(indices);
        self.num_ -= static_cast<std::int32_t>(indices.size());
    }
    template <typename Self>
    void add_defaulted(this Self& self, std::int32_t const count) {
        auto const first{self.num_};
        self.add_uninitialised(count);
        if (count > 0) {
            self.default_construct_columns(first, count);
        }
    }
    template <typename Self>
    void set_num(this Self& self, std::int32_t const count) {
        require(count >= 0);
        if (count > self.num_) {
            self.add_defaulted(count - self.num_);
        } else {
            self.num_ = count;
        }
    }
    template <typename Self>
    void remove_at_swap(this Self& self, std::int32_t const index, std::int32_t const count) {
        require(index >= 0 && index <= self.num_ && count >= 0 && count <= self.num_ - index);
        auto const tail{self.num_ - index - count};
        auto const move_count{std::min(count, tail)};
        if (move_count > 0) {
            self.swap_remove_columns(index, self.num_ - move_count, move_count);
        }
        self.num_ -= count;
    }
};

}

namespace ml::native_soa {
template <typename T>
using Vector2View = soa_storage_detail::VectorView<T, 2, std::span, require>;
template <typename T>
using Vector2ConstView = Vector2View<T const>;
template <typename T>
using Vector3View = soa_storage_detail::VectorView<T, 3, std::span, require>;
template <typename T>
using Vector3ConstView = Vector3View<T const>;
}
