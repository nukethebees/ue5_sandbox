#pragma once

#include <SbxCoreExperiments/single_allocation_layout.h>

#include <Containers/AllowShrinking.h>
#include <Containers/ContainerAllocationPolicies.h>
#include <HAL/UnrealMemory.h>
#include <Misc/AssertionMacros.h>
#include <Templates/MemoryOps.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace ml::single_allocation_experiment {

using single_allocation_layout::capacity_granularity;
using single_allocation_layout::layout_align;
using single_allocation_layout::maximum_capacity;
using single_allocation_layout::supported_leaf;
using single_allocation_layout::try_allocation_bytes;
using single_allocation_layout::try_round_capacity;
[[noreturn]] inline void invalid_size() {
    LowLevelFatalError(TEXT("Single-allocation SoA: invalid size, range, or allocation overflow."));
    std::abort();
}

inline void require(bool const condition) {
    if (!condition) {
        invalid_size();
    }
}

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

inline auto allocate(SIZE_T const bytes, uint32 const alignment) -> std::byte* {
    auto* const allocation{FMemory::Realloc(nullptr, bytes, alignment)};
    require(allocation != nullptr);
    // The byte array provides storage and starts implicit-lifetime leaf arrays without
    // initialization.
    return ::new (allocation) std::byte[bytes];
}

// Generated storage supplies the state and typed column operations.
struct StorageOperations {
    template <typename Self>
    auto num(this Self const& self) noexcept -> int32 {
        return self.num_;
    }
    template <typename Self>
    auto capacity(this Self const& self) noexcept -> int32 {
        return self.capacity_;
    }
    template <typename Self>
    auto is_empty(this Self const& self) noexcept -> bool {
        return self.num_ == 0;
    }
    template <typename Self>
    auto allocated_bytes(this Self const& self) -> SIZE_T {
        return allocation_bytes(self.capacity_, Self::block_bytes);
    }
    template <typename Self>
    void reserve(this Self& self, int32 const count) {
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
    void add_uninitialised(this Self& self, int32 const count) {
        require(count >= 0 && count <= Self::max_capacity - self.num_);
        auto const new_num{self.num_ + count};
        if (new_num > self.capacity_) {
            self.reallocate(growth_capacity(new_num, self.capacity_, Self::block_bytes));
        }
        self.num_ = new_num;
    }
    template <typename Self>
    void add_defaulted(this Self& self, int32 const count) {
        auto const first{self.num_};
        self.add_uninitialised(count);
        if (count > 0) {
            self.default_construct_columns(first, count);
        }
    }
    template <typename Self>
    void set_num(this Self& self, int32 const count, EAllowShrinking const = EAllowShrinking::No) {
        require(count >= 0);
        if (count > self.num_) {
            self.add_defaulted(count - self.num_);
        } else {
            self.num_ = count;
        }
    }
    template <typename Self>
    void remove_at_swap(this Self& self,
                        int32 const index,
                        int32 const count,
                        EAllowShrinking const = EAllowShrinking::No) {
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
