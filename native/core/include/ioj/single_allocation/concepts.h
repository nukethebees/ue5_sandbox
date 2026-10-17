#pragma once

#include <concepts>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <type_traits>

namespace ml::soa_storage_detail {

template <typename Left, typename Right>
concept SameSoaSchema = std::same_as<typename std::remove_cvref_t<Left>::soa_schema,
                                     typename std::remove_cvref_t<Right>::soa_schema>;

template <typename Source, typename Destination, typename Size>
concept SoaSourceFor = SameSoaSchema<Source, Destination> && requires(Source const& source) {
    { source.num() } -> std::convertible_to<Size>;
    source.validate();
};

template <typename Owner>
using StorageConstView = typename std::remove_cvref_t<Owner>::ConstView;
template <typename Owner>
using StorageView = std::conditional_t<std::is_const_v<std::remove_reference_t<Owner>>,
                                       StorageConstView<Owner>,
                                       typename std::remove_cvref_t<Owner>::View>;

// Evaluate private-member requirements in the owners' friend context.
struct StorageRequirements {
    /* **************************************** */
    // Size and capacity
    /* **************************************** */
    template <typename Owner, typename Size>
    static consteval auto has_size() -> bool {
        return requires(Owner const& owner) {
            { owner.num_ } -> std::convertible_to<Size>;
        };
    }
    template <typename Owner, typename Size>
    static consteval auto has_capacity() -> bool {
        return requires(Owner const& owner) {
            { owner.capacity_ } -> std::convertible_to<Size>;
        };
    }
    template <typename Owner>
    static consteval auto has_allocation_size() -> bool {
        return requires(Owner const& owner) {
            {
                Owner::layout_bytes(owner.capacity_ / Owner::capacity_granularity)
            } -> std::convertible_to<std::size_t>;
        };
    }

    /* **************************************** */
    // Allocation and mutations
    /* **************************************** */
    template <typename Owner, typename Size>
    static consteval auto can_reallocate() -> bool {
        return requires(Owner& owner, Size count, std::byte* data) {
            typename Owner::byte_size_type;
            owner.data_ = data;
            owner.capacity_ = count;
            { owner.num_ } -> std::convertible_to<Size>;
            { Owner::capacity_granularity } -> std::convertible_to<Size>;
            { Owner::capacity_block_bound } -> std::convertible_to<std::size_t>;
            { Owner::max_capacity } -> std::convertible_to<Size>;
            { Owner::allocation_alignment } -> std::convertible_to<std::size_t>;
            { Owner::layout_bytes(owner.capacity_blocks()) } -> std::convertible_to<std::size_t>;
            { owner.resource_ } -> std::convertible_to<std::pmr::memory_resource*>;
            owner.copy_live_columns(data, count);
        };
    }
    template <typename Owner, typename Size>
    static consteval auto can_reserve() -> bool {
        return requires(Owner& owner, Size count) { owner.reallocate(count); };
    }
    template <typename Owner, typename Size>
    static consteval auto can_reset() -> bool {
        return requires(Owner& owner) { owner.num_ = Size{}; };
    }
    template <typename Owner, typename Source, typename Size>
    static consteval auto can_copy_columns() -> bool {
        return requires(Owner& owner, Source const& source, Size count) {
            owner.copy_columns_from(source, count, count, count);
        };
    }
    template <typename Owner, typename Source, typename Size>
    static consteval auto can_append_columns() -> bool {
        return requires(Owner& owner, Source const& source, Size count) {
            owner.append_columns(source, count, count, count);
        };
    }
    template <typename Owner, typename Size>
    static consteval auto can_remove_indices() -> bool {
        return requires(Owner& owner, std::span<Size const> indices) {
            owner.num_ -= static_cast<Size>(indices.size());
            owner.swap_remove_indices(indices);
        };
    }
    template <typename Owner, typename Size>
    static consteval auto can_default_construct_columns() -> bool {
        return requires(Owner& owner, Size count) {
            owner.default_construct_columns(count, count);
        };
    }
    template <typename Owner, typename Size>
    static consteval auto can_remove_range() -> bool {
        return requires(Owner& owner, Size count) {
            owner.num_ -= count;
            owner.swap_remove_columns(count, count, count);
        };
    }

    /* **************************************** */
    // Borrowing
    /* **************************************** */
    template <typename Owner, typename Size>
    static consteval auto can_borrow() -> bool {
        return requires(Owner&& owner, Size count) { StorageView<Owner>{&owner, count, count}; };
    }
    template <typename Owner, typename Size>
    static consteval auto can_borrow_const() -> bool {
        return requires(Owner&& owner, Size count) {
            StorageConstView<Owner>{&owner, count, count};
        };
    }
};

template <typename Owner, typename Size>
concept SizedStorage = StorageRequirements::has_size<Owner, Size>();
template <typename Owner, typename Size>
concept CapacityStorage = StorageRequirements::has_capacity<Owner, Size>();
template <typename Owner>
concept MeasurableStorage = StorageRequirements::has_allocation_size<Owner>();
template <typename Owner, typename Size>
concept ReallocatableStorage = StorageRequirements::can_reallocate<Owner, Size>();
template <typename Owner, typename Size>
concept ReservableStorage = StorageRequirements::can_reserve<Owner, Size>();
template <typename Owner, typename Size>
concept ResettableStorage = StorageRequirements::can_reset<Owner, Size>();
template <typename Owner, typename Size>
concept GrowableStorage = ResettableStorage<Owner, Size> && ReservableStorage<Owner, Size>;
template <typename Owner, typename Source, typename Size>
concept CopyableStorage =
    SizedStorage<Owner, Size> && StorageRequirements::can_copy_columns<Owner, Source, Size>();
template <typename Owner, typename Source, typename Size>
concept AppendableStorage =
    GrowableStorage<Owner, Size> && StorageRequirements::can_append_columns<Owner, Source, Size>();
template <typename Owner, typename Size>
concept IndexRemovableStorage = StorageRequirements::can_remove_indices<Owner, Size>();
template <typename Owner, typename Size>
concept DefaultableStorage = requires(Owner& owner, Size count) {
    owner.add_uninitialised(count);
} && StorageRequirements::can_default_construct_columns<Owner, Size>();
template <typename Owner, typename Size>
concept ResizableStorage = requires(Owner& owner, Size count) { owner.add_defaulted(count); };
template <typename Owner, typename Size>
concept RangeRemovableStorage = StorageRequirements::can_remove_range<Owner, Size>();

template <typename Source, typename Destination, typename Size>
concept CopyableTo = requires(Destination& destination, Source const& source, Size count) {
    destination.copy_elements(count, source, count, count);
};
template <typename Source, typename Destination>
concept AppendableTo =
    requires(Destination& destination, Source const& source) { destination.append_from(source); };

template <typename Owner, typename Size>
concept BorrowableStorage =
    std::is_lvalue_reference_v<Owner> && SizedStorage<std::remove_reference_t<Owner>, Size> &&
    StorageRequirements::can_borrow<Owner, Size>();
template <typename Owner, typename Size>
concept ConstBorrowableStorage =
    std::is_lvalue_reference_v<Owner> && SizedStorage<std::remove_reference_t<Owner>, Size> &&
    StorageRequirements::can_borrow_const<Owner, Size>();
template <typename Owner, typename Size>
concept SliceableStorage = std::is_lvalue_reference_v<Owner> &&
                           requires(Owner&& owner, Size count) { owner.get_view(count, count); };
template <typename Owner, typename Size>
concept ConstSliceableStorage =
    std::is_lvalue_reference_v<Owner> &&
    requires(Owner&& owner, Size count) { owner.get_const_view(count, count); };
template <typename Owner, typename Size>
concept LeftBorrowableStorage =
    std::is_lvalue_reference_v<Owner> &&
    requires(Owner&& owner, Size count) { owner.get_view().left(count); };
template <typename Owner, typename Size>
concept RightBorrowableStorage =
    std::is_lvalue_reference_v<Owner> &&
    requires(Owner&& owner, Size count) { owner.get_view().right(count); };

} // namespace ml::soa_storage_detail
