#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>

namespace ml::single_allocation_layout {

struct LayoutPolicy {
    inline static constexpr std::int32_t capacity_granularity{64};
    inline static constexpr std::size_t column_gap{192};
    inline static constexpr std::size_t minimum_alignment{64};
};

inline constexpr auto capacity_granularity{LayoutPolicy::capacity_granularity};

template <typename T>
inline constexpr bool supported_leaf =
    std::is_object_v<T> && !std::is_array_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T> &&
    std::is_trivially_copyable_v<T> && std::is_trivially_copy_constructible_v<T> &&
    std::is_trivially_destructible_v<T> && std::is_nothrow_default_constructible_v<T>;

constexpr auto layout_align(std::size_t const bytes, std::size_t const alignment) -> std::size_t {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0 ||
        bytes > std::numeric_limits<std::size_t>::max() - (alignment - 1)) {
        std::abort();
    }
    return (bytes + alignment - 1) & ~(alignment - 1);
}

struct ColumnLayoutBase {
  protected:
    constexpr ColumnLayoutBase() noexcept
        : capacity_granularity{LayoutPolicy::capacity_granularity}
        , column_gap{LayoutPolicy::column_gap}
        , minimum_alignment{LayoutPolicy::minimum_alignment} {}

    constexpr ColumnLayoutBase(ColumnLayoutBase const& previous,
                               std::size_t const element_size,
                               std::size_t const element_alignment) noexcept
        : capacity_granularity{previous.capacity_granularity}
        , column_gap{previous.column_gap}
        , minimum_alignment{previous.minimum_alignment}
        , alignment{element_alignment > minimum_alignment ? element_alignment : minimum_alignment}
        , block_offset{layout_align(previous.block_end, alignment)}
        , block_end{checked_block_end(block_offset, capacity_granularity, element_size)}
        , column_count{previous.column_count + 1}
        , allocation_alignment{previous.allocation_alignment > alignment
                                   ? previous.allocation_alignment
                                   : alignment}
        , element_size_{element_size}
        , previous_{&previous} {}
  public:
    std::size_t capacity_granularity{};
    std::size_t column_gap{};
    std::size_t minimum_alignment{};
    std::size_t alignment{1};
    std::size_t block_offset{};
    std::size_t block_end{};
    std::size_t column_count{};
    std::size_t allocation_alignment{1};

    constexpr auto offset_after(std::size_t const previous_next) const noexcept -> std::size_t {
        return layout_align(previous_next, alignment);
    }

    constexpr auto data_end_at(std::size_t const byte_offset,
                               std::size_t const blocks) const noexcept -> std::size_t {
        return byte_offset + blocks * capacity_granularity * element_size_;
    }

    constexpr auto next_offset_at(std::size_t const byte_offset,
                                  std::size_t const blocks) const noexcept -> std::size_t {
        return data_end_at(byte_offset, blocks) + column_gap;
    }

    constexpr auto offset(std::size_t const blocks) const noexcept -> std::size_t {
        return previous_ == nullptr ? 0 : offset_after(previous_->next_offset(blocks));
    }
    constexpr auto data_end(std::size_t const blocks) const noexcept -> std::size_t {
        return data_end_at(offset(blocks), blocks);
    }
    constexpr auto next_offset(std::size_t const blocks) const noexcept -> std::size_t {
        return previous_ == nullptr ? 0 : next_offset_at(offset(blocks), blocks);
    }
  private:
    static constexpr auto checked_block_end(std::size_t const offset,
                                            std::size_t const granularity,
                                            std::size_t const element_size) noexcept
        -> std::size_t {
        if (element_size > (std::numeric_limits<std::size_t>::max() - offset) / granularity) {
            std::abort();
        }
        return offset + granularity * element_size;
    }

    std::size_t element_size_{};
    ColumnLayoutBase const* previous_{};
};

struct ColumnLayoutStart : ColumnLayoutBase {
    constexpr ColumnLayoutStart() noexcept = default;
};

template <typename T>
struct ColumnLayout : ColumnLayoutBase {
    static_assert(supported_leaf<T>,
                  "Single-allocation leaf requires a non-cv trivially copyable, trivially "
                  "destructible, nothrow default-constructible object type");
    using value_type = T;
    using pointer = T*;
    using const_pointer = T const*;

    // This intentionally chains same-type columns instead of copying their offsets. A constructor
    // template alone would lose to an implicitly generated copy constructor.
    constexpr explicit ColumnLayout(ColumnLayout const& previous_column) noexcept
        : ColumnLayoutBase{previous_column, sizeof(T), alignof(T)} {}

    template <typename PreviousColumn>
        requires (!std::is_same_v<std::remove_cvref_t<PreviousColumn>, ColumnLayout>)
    constexpr explicit ColumnLayout(PreviousColumn& previous_column) noexcept
        : ColumnLayoutBase{previous_column, sizeof(T), alignof(T)} {}

    auto operator=(ColumnLayout const&) -> ColumnLayout& = delete;
};

struct LayoutCursor {
    explicit constexpr LayoutCursor(std::size_t const blocks) noexcept
        : blocks_{blocks} {}

    constexpr auto advance(ColumnLayoutBase const& column) noexcept -> std::size_t {
        auto const byte_offset{column.offset_after(next_offset_)};
        next_offset_ = column.next_offset_at(byte_offset, blocks_);
        return byte_offset;
    }
  private:
    std::size_t blocks_{};
    std::size_t next_offset_{};
};

constexpr auto capacity_block_bound(ColumnLayoutBase const& last) noexcept -> std::size_t {
    if (last.column_count == 0) {
        return 0;
    }
    auto const alignment{last.allocation_alignment};
    auto const aligned_end{layout_align(last.block_end, alignment)};
    auto const per_gap{last.column_gap + alignment - 1};
    auto const gaps{last.column_count - 1};
    if (gaps > (std::numeric_limits<std::size_t>::max() - aligned_end) / per_gap) {
        std::abort();
    }
    return aligned_end + gaps * per_gap;
}

template <typename... Columns>
constexpr auto maximum_alignment(Columns const&... columns) noexcept -> std::size_t {
    std::size_t result{1};
    ((result = result < columns.alignment ? columns.alignment : result), ...);
    return result;
}

constexpr auto maximum_capacity(std::size_t const block_bytes) noexcept -> std::int32_t {
    if (block_bytes == 0) {
        return 0;
    }
    auto const integer_blocks{
        static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max() / capacity_granularity)};
    auto const address_blocks{static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) /
                              block_bytes};
    auto const blocks{integer_blocks < address_blocks ? integer_blocks : address_blocks};
    return static_cast<std::int32_t>(blocks * capacity_granularity);
}

constexpr auto try_round_capacity(std::int64_t const required,
                                  std::int32_t const maximum,
                                  std::int32_t& result) noexcept -> bool {
    if (required < 0 || required > maximum) {
        return false;
    }
    auto const rounded{((required + capacity_granularity - 1) / capacity_granularity) *
                       capacity_granularity};
    if (rounded > maximum) {
        return false;
    }
    result = static_cast<std::int32_t>(rounded);
    return true;
}

constexpr auto try_allocation_bytes(std::int32_t const capacity,
                                    std::size_t const block_bytes,
                                    std::size_t& result) noexcept -> bool {
    if (capacity < 0 || capacity % capacity_granularity != 0 ||
        capacity > maximum_capacity(block_bytes)) {
        return false;
    }
    result = static_cast<std::size_t>(capacity / capacity_granularity) * block_bytes;
    return true;
}

}
