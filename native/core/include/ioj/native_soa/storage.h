#pragma once

#include <ioj/single_allocation/layout.h>
#include <ioj/single_allocation/operations.h>
#include <ioj/single_allocation/view.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <memory_resource>
#include <span>
#include <type_traits>
#include <utility>

namespace ioj::native_soa {

using size_type = std::uint32_t;
using byte_size_type = std::size_t;

using single_allocation_layout::capacity_granularity;
using single_allocation_layout::layout_align;
using single_allocation_layout::LayoutPolicy;
using single_allocation_layout::maximum_capacity;
using single_allocation_layout::supported_leaf;
using single_allocation_layout::try_allocation_bytes;
using single_allocation_layout::try_round_capacity;
using soa_storage_detail::source_data;

using StorageState = soa_storage_detail::StorageState;

inline auto rounded_capacity(size_type const required,
                             [[maybe_unused]] byte_size_type const block_bytes) -> size_type {
    assert(required <= maximum_capacity(block_bytes));
    auto const rounded{
        ((byte_size_type{required} + capacity_granularity - 1) / capacity_granularity) *
        capacity_granularity};
    return static_cast<size_type>(rounded);
}

inline auto growth_capacity(size_type const required,
                            size_type const current,
                            byte_size_type const block_bytes) -> size_type {
    auto const maximum{byte_size_type{maximum_capacity(block_bytes)}};
    assert(required > current && required <= maximum);
    auto const wide_current{byte_size_type{current}};
    auto const geometric{std::max<byte_size_type>(required, wide_current + wide_current / 2)};
    auto const bounded{std::min(geometric, maximum)};
    return rounded_capacity(static_cast<size_type>(bounded), block_bytes);
}

using StorageOperations = soa_storage_detail::StorageOperations<rounded_capacity, growth_capacity>;

template <typename MutableView, typename ReadOnlyView>
class Owner
    : protected StorageState
    , public StorageOperations {
  public:
    using soa_schema = typename MutableView::soa_schema;
    using Layout = typename MutableView::Layout;
    using CompactView = MutableView;
    using CompactConstView = ReadOnlyView;
    using size_type = Layout::size_type;
    using byte_size_type = Layout::byte_size_type;
    static constexpr auto capacity_granularity{Layout::capacity_granularity};
    static constexpr auto allocation_alignment{Layout::allocation_alignment};
    static constexpr auto capacity_block_bound{Layout::capacity_block_bound};
    static constexpr auto max_capacity{Layout::max_capacity};

    Owner() noexcept
        : Owner{*std::pmr::get_default_resource()} {}
    explicit Owner(std::pmr::memory_resource& resource) noexcept
        : resource_{&resource} {}
    ~Owner() { release_storage(*this); }
    Owner(Owner const&) = delete;
    Owner(Owner&& other) noexcept
        : resource_{other.resource_} {
        take_storage(*this, other);
    }
    auto operator=(Owner const&) -> Owner& = delete;
    auto operator=(Owner&& other) -> Owner& { return move_assign(*this, other); }

    static constexpr auto layout_bytes(byte_size_type blocks) noexcept -> byte_size_type {
        return Layout::layout_bytes(blocks);
    }
    auto memory_resource() const noexcept -> std::pmr::memory_resource* { return resource_; }

  private:
    friend StorageOperations;
    friend soa_storage_detail::StorageRequirements;

    std::pmr::memory_resource* resource_;
};

}
