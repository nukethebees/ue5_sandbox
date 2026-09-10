#pragma once

#include <SandboxCore/compact_vector_view.h>
#include <SandboxCore/single_allocation_view.h>

#include <SandboxCore/mimalloc_storage_allocator.h>
#include <SandboxCore/single_allocation_layout.h>

#include <Containers/AllowShrinking.h>
#include <Containers/ArrayView.h>
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

namespace ml::soa_storage {

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
    LowLevelFatalError(TEXT("Single-allocation SoA: invalid size, range, or allocation overflow."));
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

struct FMemoryStorageAllocator {
    static auto allocate(SIZE_T const bytes, uint32 const alignment) -> std::byte* {
        auto* const allocation{FMemory::Realloc(nullptr, bytes, alignment)};
        require(allocation != nullptr);
        // The byte array provides storage and starts implicit-lifetime leaf arrays without
        // initialization.
        return ::new (allocation) std::byte[bytes];
    }
    static void free(std::byte* data) noexcept { FMemory::Free(data); }
};

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
        return Self::layout_bytes(
            static_cast<std::size_t>(self.capacity_ / Self::capacity_granularity));
    }
    template <typename Self>
    void reserve(this Self& self, int32 const count) {
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
    void add_uninitialised(this Self& self, int32 const count) {
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
    void remove_at_swap(this Self& self, TConstArrayView<int32> indices) {
        self.remove_at_swap(
            std::span<int32 const>{indices.GetData(), static_cast<SIZE_T>(indices.Num())});
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

namespace ml::soa {
template <typename T>
using Vector2View = soa_storage_detail::VectorView<T, 2, TArrayView, soa_storage::require>;
template <typename T>
using Vector2ConstView = Vector2View<T const>;
template <typename T>
using Vector3View = soa_storage_detail::VectorView<T, 3, TArrayView, soa_storage::require>;
template <typename T>
using Vector3ConstView = Vector3View<T const>;
}
