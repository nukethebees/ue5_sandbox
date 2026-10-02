#pragma once

#include <algorithm>
#include <cstddef>
#include <memory_resource>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

namespace ml::soa_storage_detail {

// Generated owners supply state and explicit typed column operations.
template <typename Size, auto Require, auto RoundedCapacity, auto GrowthCapacity>
struct StorageOperations {
    /* **************************************** */
    // Lifetime
    /* **************************************** */
    template <typename Owner>
    static void release_storage(Owner& owner) noexcept {
        if (owner.data_ != nullptr) {
            owner.resource_->deallocate(owner.data_,
                                        owner.layout_bytes(owner.capacity_blocks()),
                                        Owner::allocation_alignment);
        }
    }

    // Transfer into a destination with no live allocation, preserving both resources.
    template <typename Owner>
    static void take_storage(Owner& destination, Owner& source) noexcept {
        destination.data_ = std::exchange(source.data_, nullptr);
        destination.num_ = std::exchange(source.num_, 0);
        destination.capacity_ = std::exchange(source.capacity_, 0);
    }

    template <typename Owner>
    static auto move_assign(Owner& destination, Owner& source) -> Owner& {
        if (&destination == &source) {
            return destination;
        }

        if (*destination.resource_ == *source.resource_) {
            release_storage(destination);
            take_storage(destination, source);
        } else {
            // Keep both owners intact until allocation succeeds.
            if (source.num_ > destination.capacity_) {
                destination.reallocate(source.capacity_);
            }
            if (source.num_ > 0) {
                destination.append_columns(source.get_const_view(), 0, 0, source.num_);
            }
            destination.num_ = std::exchange(source.num_, 0);
        }

        return destination;
    }

    /* **************************************** */
    // Allocation
    /* **************************************** */
    template <typename Owner>
    static auto allocate_storage(Owner& owner, Size const capacity) -> std::byte* {
        auto const bytes{Owner::layout_bytes(
            static_cast<typename Owner::byte_size_type>(capacity / Owner::capacity_granularity))};
        auto* const allocation{owner.resource_->allocate(bytes, Owner::allocation_alignment)};
        // Start the implicit-lifetime column arrays without initializing rows.
        return ::new (allocation) std::byte[bytes];
    }

    template <typename Self>
    void reallocate(this Self& self, Size const new_capacity) {
        auto* const new_data{allocate_storage(self, new_capacity)};
        if (self.num_ > 0) {
            self.copy_live_columns(new_data, new_capacity);
        }
        release_storage(self);
        self.data_ = new_data;
        self.capacity_ = new_capacity;
    }

    /* **************************************** */
    // Capacity and mutations
    /* **************************************** */
    template <typename Self>
    auto num(this Self const& self) noexcept -> Size {
        return self.num_;
    }
    template <typename Self>
    auto capacity(this Self const& self) noexcept -> Size {
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
    void reserve(this Self& self, Size const count) {
        auto const requested{RoundedCapacity(count, Self::capacity_block_bound)};
        if (requested > self.capacity_) {
            self.reallocate(requested);
        }
    }
    template <typename Self>
    void reset(this Self& self) noexcept {
        self.num_ = 0;
    }
    template <typename Self>
    void add_uninitialised(this Self& self, Size const count) {
        Require(count >= 0 && count <= Self::max_capacity - self.num_);
        auto const new_num{self.num_ + count};
        if (new_num > self.capacity_) {
            self.reallocate(GrowthCapacity(new_num, self.capacity_, Self::capacity_block_bound));
        }
        self.num_ = new_num;
    }
    template <typename Self, typename Source>
        requires (Self::template accepts_source<Source>)
    void copy_elements(this Self& self,
                       Size const destination,
                       Source const& source,
                       Size const offset,
                       Size const count) {
        source.validate();
        Require(destination >= 0 && destination <= self.num_ && count >= 0 &&
                count <= self.num_ - destination && offset >= 0 && offset <= source.num() &&
                count <= source.num() - offset);
        if (count > 0) {
            self.copy_columns_from(source, offset, destination, count);
        }
    }
    template <typename Self, typename Source>
        requires (Self::template accepts_source<Source>)
    void copy_element(this Self& self,
                      Size const destination,
                      Source const& source,
                      Size const offset) {
        self.copy_elements(destination, source, offset, 1);
    }
    template <typename Self, typename Source>
        requires (Self::template accepts_source<Source>)
    auto append_from(this Self& self, Source const& source) -> Size {
        return self.append_from(source, 0, source.num());
    }
    template <typename Self, typename Source>
        requires (Self::template accepts_source<Source>)
    auto append_from(this Self& self, Source const& source, Size const offset, Size const count)
        -> Size {
        source.validate();
        Require(offset >= 0 && offset <= source.num() && count >= 0 &&
                count <= source.num() - offset);
        auto const first{self.num_};
        Require(count >= 0 && count <= Self::max_capacity - first);
        if (count == 0) {
            return first;
        }
        auto const new_num{first + count};
        if (new_num > self.capacity_) {
            self.reallocate(GrowthCapacity(new_num, self.capacity_, Self::capacity_block_bound));
        }
        self.append_columns(source, offset, first, count);
        self.num_ = new_num;
        return first;
    }
    template <typename Self>
    void remove_at_swap(this Self& self, std::span<Size const> indices) {
        self.swap_remove_indices(indices);
        self.num_ -= static_cast<Size>(indices.size());
    }
    template <typename Self>
    void add_defaulted(this Self& self, Size const count) {
        auto const first{self.num_};
        self.add_uninitialised(count);
        if (count > 0) {
            self.default_construct_columns(first, count);
        }
    }
    template <typename Self>
    void set_num(this Self& self, Size const count) {
        Require(count >= 0);
        if (count > self.num_) {
            self.add_defaulted(count - self.num_);
        } else {
            self.num_ = count;
        }
    }
    template <typename Self>
    void remove_at_swap(this Self& self, Size const index, Size const count) {
        Require(index >= 0 && index <= self.num_ && count >= 0 && count <= self.num_ - index);
        auto const tail{self.num_ - index - count};
        auto const move_count{std::min(count, tail)};
        if (move_count > 0) {
            self.swap_remove_columns(index, self.num_ - move_count, move_count);
        }
        self.num_ -= count;
    }

    /* **************************************** */
    // Borrowing
    /* **************************************** */
    template <typename Self>
        requires std::is_lvalue_reference_v<Self>
    auto get_view(this Self&& self) {
        return self.get_view(0, self.num_);
    }
    template <typename Self>
        requires std::is_lvalue_reference_v<Self>
    auto get_view(this Self&& self, Size offset, Size count) {
        using Owner = std::remove_cvref_t<Self>;
        using View = std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>,
                                        typename Owner::ConstView,
                                        typename Owner::View>;
        return View{&self, offset, count};
    }
    template <typename Self>
        requires std::is_lvalue_reference_v<Self>
    auto get_const_view(this Self&& self) {
        return self.get_const_view(0, self.num_);
    }
    template <typename Self>
        requires std::is_lvalue_reference_v<Self>
    auto get_const_view(this Self&& self, Size offset, Size count) {
        using View = typename std::remove_cvref_t<Self>::ConstView;
        return View{&self, offset, count};
    }
    template <typename Self>
        requires std::is_lvalue_reference_v<Self>
    auto slice(this Self&& self, Size offset, Size count) {
        return self.get_view(offset, count);
    }
    template <typename Self>
        requires std::is_lvalue_reference_v<Self>
    auto left(this Self&& self, Size count) {
        return self.get_view().left(count);
    }
    template <typename Self>
        requires std::is_lvalue_reference_v<Self>
    auto right(this Self&& self, Size count) {
        return self.get_view().right(count);
    }
};

} // namespace ml::soa_storage_detail
