#pragma once

#include <SandboxCore/single_allocation/runtime.h>

#include <algorithm>
#include <Containers/AllowShrinking.h>
#include <Containers/ArrayView.h>
#include <Containers/ContainerAllocationPolicies.h>
#include <span>

namespace ml::soa_storage {
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
