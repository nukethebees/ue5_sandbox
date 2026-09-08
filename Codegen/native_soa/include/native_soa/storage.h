#pragma once

#include <SbxCoreExperiments/single_allocation_layout.h>

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

namespace ml::native_soa {

using single_allocation_layout::capacity_granularity;
using single_allocation_layout::layout_align;
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
    auto* const allocation{::operator new(bytes, std::align_val_t{alignment})};
    // The byte array provides storage and starts implicit-lifetime leaf arrays without
    // initialization.
    return ::new (allocation) std::byte[bytes];
}

inline void free(std::byte* data, std::size_t alignment) noexcept {
    ::operator delete(data, std::align_val_t{alignment});
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
        return allocation_bytes(self.capacity_, Self::block_bytes);
    }
    template <typename Self>
    void reserve(this Self& self, std::int32_t const count) {
        auto const requested{rounded_capacity(count, Self::block_bytes)};
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
            self.reallocate(growth_capacity(new_num, self.capacity_, Self::block_bytes));
        }
        self.num_ = new_num;
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
