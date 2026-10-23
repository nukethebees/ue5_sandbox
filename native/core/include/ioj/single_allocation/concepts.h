#pragma once

#include <concepts>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <type_traits>

namespace ioj::soa_storage_detail {

template <typename Owner>
using StorageSize = typename std::remove_cvref_t<Owner>::size_type;

template <typename Owner>
concept StorageOwner = requires { typename StorageSize<Owner>; } &&
                       std::integral<StorageSize<Owner>> && !std::same_as<StorageSize<Owner>, bool>;

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
    template <StorageOwner Owner>
    static consteval auto has_size() -> bool {
        return requires(Owner const& owner) {
            { owner.num_ } -> std::convertible_to<StorageSize<Owner>>;
        };
    }
    template <StorageOwner Owner>
    static consteval auto has_capacity() -> bool {
        return requires(Owner const& owner) {
            { owner.capacity_ } -> std::convertible_to<StorageSize<Owner>>;
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
    template <StorageOwner Owner>
    static consteval auto can_reallocate() -> bool {
        return requires(Owner& owner, StorageSize<Owner> count, std::byte* data) {
            typename Owner::byte_size_type;
            owner.data_ = data;
            owner.capacity_ = count;
            { owner.num_ } -> std::convertible_to<StorageSize<Owner>>;
            { Owner::capacity_granularity } -> std::convertible_to<StorageSize<Owner>>;
            { Owner::capacity_block_bound } -> std::convertible_to<std::size_t>;
            { Owner::max_capacity } -> std::convertible_to<StorageSize<Owner>>;
            { Owner::allocation_alignment } -> std::convertible_to<std::size_t>;
            { Owner::layout_bytes(owner.capacity_blocks()) } -> std::convertible_to<std::size_t>;
            { owner.resource_ } -> std::convertible_to<std::pmr::memory_resource*>;
            owner.copy_live_columns(data, count);
        };
    }
    template <StorageOwner Owner>
    static consteval auto can_reserve() -> bool {
        return requires(Owner& owner, StorageSize<Owner> count) { owner.reallocate(count); };
    }
    template <StorageOwner Owner>
    static consteval auto can_reset() -> bool {
        return requires(Owner& owner) { owner.num_ = StorageSize<Owner>{}; };
    }
    template <StorageOwner Owner, typename Source>
    static consteval auto can_copy_columns() -> bool {
        return requires(Owner& owner, Source const& source, StorageSize<Owner> count) {
            owner.copy_columns_from(source, count, count, count);
        };
    }
    template <StorageOwner Owner, typename Source>
    static consteval auto can_append_columns() -> bool {
        return requires(Owner& owner, Source const& source, StorageSize<Owner> count) {
            owner.append_columns(source, count, count, count);
        };
    }
    template <StorageOwner Owner>
    static consteval auto can_remove_indices() -> bool {
        return requires(Owner& owner, std::span<StorageSize<Owner> const> indices) {
            owner.num_ -= static_cast<StorageSize<Owner>>(indices.size());
            owner.swap_remove_indices(indices);
        };
    }
    template <StorageOwner Owner>
    static consteval auto can_default_construct_columns() -> bool {
        return requires(Owner& owner, StorageSize<Owner> count) {
            owner.default_construct_columns(count, count);
        };
    }
    template <StorageOwner Owner>
    static consteval auto can_remove_range() -> bool {
        return requires(Owner& owner, StorageSize<Owner> count) {
            owner.num_ -= count;
            owner.swap_remove_columns(count, count, count);
        };
    }

    /* **************************************** */
    // Borrowing
    /* **************************************** */
    template <StorageOwner Owner>
    static consteval auto can_borrow() -> bool {
        return requires(Owner&& owner, StorageSize<Owner> count) {
            StorageView<Owner>{&owner, count, count};
        };
    }
    template <StorageOwner Owner>
    static consteval auto can_borrow_const() -> bool {
        return requires(Owner&& owner, StorageSize<Owner> count) {
            StorageConstView<Owner>{&owner, count, count};
        };
    }
};

template <typename Owner>
concept SizedStorage = StorageOwner<Owner> && StorageRequirements::has_size<Owner>();
template <typename Owner>
concept CapacityStorage = StorageOwner<Owner> && StorageRequirements::has_capacity<Owner>();
template <typename Owner>
concept MeasurableStorage =
    StorageOwner<Owner> && StorageRequirements::has_allocation_size<Owner>();
template <typename Owner>
concept ReallocatableStorage = StorageOwner<Owner> && StorageRequirements::can_reallocate<Owner>();
template <typename Owner>
concept ReservableStorage = StorageOwner<Owner> && StorageRequirements::can_reserve<Owner>();
template <typename Owner>
concept ResettableStorage = StorageOwner<Owner> && StorageRequirements::can_reset<Owner>();
template <typename Owner>
concept GrowableStorage = ResettableStorage<Owner> && ReservableStorage<Owner>;
template <typename Owner, typename Source>
concept CopyableStorage =
    SizedStorage<Owner> && StorageRequirements::can_copy_columns<Owner, Source>();
template <typename Owner, typename Source>
concept AppendableStorage =
    GrowableStorage<Owner> && StorageRequirements::can_append_columns<Owner, Source>();
template <typename Owner>
concept IndexRemovableStorage =
    StorageOwner<Owner> && StorageRequirements::can_remove_indices<Owner>();
template <typename Owner>
concept DefaultableStorage =
    StorageOwner<Owner> && requires(Owner& owner, StorageSize<Owner> count) {
        owner.add_uninitialised(count);
    } && StorageRequirements::can_default_construct_columns<Owner>();
template <typename Owner>
concept ResizableStorage = StorageOwner<Owner> && requires(Owner& owner, StorageSize<Owner> count) {
    owner.add_defaulted(count);
};
template <typename Owner>
concept RangeRemovableStorage =
    StorageOwner<Owner> && StorageRequirements::can_remove_range<Owner>();

template <typename Source, typename Destination, typename Size>
concept CopyableTo = requires(Destination& destination, Source const& source, Size count) {
    destination.copy_elements(count, source, count, count);
};
template <typename Source, typename Destination>
concept AppendableTo =
    requires(Destination& destination, Source const& source) { destination.append_from(source); };

template <typename Owner>
concept BorrowableStorage =
    std::is_lvalue_reference_v<Owner> && SizedStorage<std::remove_reference_t<Owner>> &&
    StorageRequirements::can_borrow<Owner>();
template <typename Owner>
concept ConstBorrowableStorage =
    std::is_lvalue_reference_v<Owner> && SizedStorage<std::remove_reference_t<Owner>> &&
    StorageRequirements::can_borrow_const<Owner>();
template <typename Owner>
concept SliceableStorage =
    StorageOwner<Owner> && std::is_lvalue_reference_v<Owner> &&
    requires(Owner&& owner, StorageSize<Owner> count) { owner.get_view(count, count); };
template <typename Owner>
concept ConstSliceableStorage =
    StorageOwner<Owner> && std::is_lvalue_reference_v<Owner> &&
    requires(Owner&& owner, StorageSize<Owner> count) { owner.get_const_view(count, count); };
template <typename Owner>
concept LeftBorrowableStorage =
    StorageOwner<Owner> && std::is_lvalue_reference_v<Owner> &&
    requires(Owner&& owner, StorageSize<Owner> count) { owner.get_view().left(count); };
template <typename Owner>
concept RightBorrowableStorage =
    StorageOwner<Owner> && std::is_lvalue_reference_v<Owner> &&
    requires(Owner&& owner, StorageSize<Owner> count) { owner.get_view().right(count); };

} // namespace ioj::soa_storage_detail
