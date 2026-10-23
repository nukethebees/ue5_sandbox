#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>

namespace ioj::single_allocation_layout {

struct LayoutPolicy {
    static constexpr std::uint32_t capacity_granularity{64};
    static constexpr std::size_t column_gap{192};
    static constexpr std::size_t minimum_alignment{64};
};

inline constexpr auto capacity_granularity{LayoutPolicy::capacity_granularity};

template <typename T>
inline constexpr bool supported_leaf =
    std::is_object_v<T> && !std::is_array_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T> &&
    std::is_trivially_copyable_v<T> && std::is_trivially_copy_constructible_v<T> &&
    std::is_trivially_destructible_v<T> && std::is_nothrow_default_constructible_v<T>;

constexpr auto layout_align(std::size_t const bytes, std::size_t const alignment) -> std::size_t {
    assert(alignment != 0 && (alignment & (alignment - 1)) == 0);
    assert(bytes <= std::numeric_limits<std::size_t>::max() - (alignment - 1));
    return (bytes + alignment - 1) & ~(alignment - 1);
}

constexpr auto maximum_capacity(std::size_t const block_bytes) noexcept -> std::uint32_t {
    if (block_bytes == 0) {
        return 0;
    }
    auto const integer_blocks{
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max() / capacity_granularity)};
    auto const address_blocks{static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) /
                              block_bytes};
    auto const blocks{integer_blocks < address_blocks ? integer_blocks : address_blocks};
    return static_cast<std::uint32_t>(blocks * capacity_granularity);
}

struct ColumnMetadata {
    std::size_t element_size{};
    std::size_t alignment{};
    std::size_t block_offset{};
    std::size_t gap_offset{};
};

template <std::size_t Count>
struct LayoutMetadata {
    std::array<ColumnMetadata, Count> columns{};
    std::size_t first_non_linear{Count};
    std::size_t allocation_alignment{1};
    std::size_t capacity_block_bound{};

    constexpr auto offset(std::size_t const index, std::size_t const blocks) const noexcept
        -> std::size_t {
        auto const direct = [&](std::size_t column) {
            return columns[column].block_offset * blocks + columns[column].gap_offset;
        };
        if (index < first_non_linear) {
            return direct(index);
        }

        // Start at the last linear column; only the remaining alignments depend on capacity.
        auto const anchor{first_non_linear - 1};
        auto next{direct(anchor) + blocks * capacity_granularity * columns[anchor].element_size};
        for (auto column{first_non_linear}; column < index; ++column) {
            auto const start{
                layout_align(next + LayoutPolicy::column_gap, columns[column].alignment)};
            next = start + blocks * capacity_granularity * columns[column].element_size;
        }
        return layout_align(next + LayoutPolicy::column_gap, columns[index].alignment);
    }
};

template <typename... Ts>
consteval auto make_column_layouts() -> LayoutMetadata<sizeof...(Ts)> {
    static_assert(sizeof...(Ts) > 0);
    static_assert((supported_leaf<Ts> && ...),
                  "Single-allocation leaf requires a non-cv trivially copyable, trivially "
                  "destructible, nothrow default-constructible object type");
    LayoutMetadata<sizeof...(Ts)> result{
        .columns = {ColumnMetadata{.element_size = sizeof(Ts),
                                   .alignment = alignof(Ts) > LayoutPolicy::minimum_alignment
                                                  ? alignof(Ts)
                                                  : LayoutPolicy::minimum_alignment}...}};
    std::size_t block_end{};
    std::size_t gap_end{};
    for (std::size_t index{}; index < sizeof...(Ts); ++index) {
        auto& column{result.columns[index]};
        if (block_end % column.alignment != 0 && result.first_non_linear == sizeof...(Ts)) {
            result.first_non_linear = index;
        }
        column.block_offset = layout_align(block_end, column.alignment);
        column.gap_offset = layout_align(gap_end, column.alignment);
        assert(column.element_size <=
               (std::numeric_limits<std::size_t>::max() - column.block_offset) /
                   capacity_granularity);
        block_end = column.block_offset + capacity_granularity * column.element_size;
        gap_end = column.gap_offset + LayoutPolicy::column_gap;
        if (column.alignment > result.allocation_alignment) {
            result.allocation_alignment = column.alignment;
        }
    }
    auto const aligned_end{layout_align(block_end, result.allocation_alignment)};
    auto const per_gap{LayoutPolicy::column_gap + result.allocation_alignment - 1};
    constexpr auto gaps{sizeof...(Ts) - 1};
    assert(gaps <= (std::numeric_limits<std::size_t>::max() - aligned_end) / per_gap);
    result.capacity_block_bound = aligned_end + gaps * per_gap;
    return result;
}

struct LayoutCursor {
    explicit constexpr LayoutCursor(std::size_t blocks) noexcept
        : blocks_{blocks} {}

    constexpr auto advance(std::size_t element_size, std::size_t alignment) noexcept
        -> std::size_t {
        auto const byte_offset{layout_align(next_offset_,
                                            alignment > LayoutPolicy::minimum_alignment
                                                ? alignment
                                                : LayoutPolicy::minimum_alignment)};
        next_offset_ =
            byte_offset + blocks_ * capacity_granularity * element_size + LayoutPolicy::column_gap;
        return byte_offset;
    }

    template <typename T, typename Byte>
    auto column_pointer(Byte* data) noexcept {
        using Pointer = std::conditional_t<std::is_const_v<Byte>, T const*, T*>;
        return std::launder(reinterpret_cast<Pointer>(data + advance(sizeof(T), alignof(T))));
    }
  private:
    std::size_t blocks_{};
    std::size_t next_offset_{};
};

template <typename... Ts>
struct ColumnLayouts {
    using size_type = std::uint32_t;
    using byte_size_type = std::size_t;
    static constexpr auto num_columns{sizeof...(Ts)};
    static constexpr auto metadata{make_column_layouts<Ts...>()};
    static constexpr auto capacity_granularity{LayoutPolicy::capacity_granularity};
    static constexpr auto column_gap{LayoutPolicy::column_gap};
    static constexpr auto allocation_alignment{metadata.allocation_alignment};
    static constexpr auto capacity_block_bound{metadata.capacity_block_bound};
    static constexpr auto max_capacity{maximum_capacity(capacity_block_bound)};

    static constexpr auto offset(std::size_t index, byte_size_type blocks) noexcept
        -> byte_size_type {
        return metadata.offset(index, blocks);
    }
    static constexpr auto layout_bytes(byte_size_type blocks) noexcept -> byte_size_type {
        return blocks == 0
                 ? 0
                 : offset(num_columns - 1, blocks) +
                       blocks * capacity_granularity * metadata.columns.back().element_size;
    }
    template <typename Result, typename Byte>
    static auto make_data(Byte* data, byte_size_type blocks) noexcept -> Result {
        LayoutCursor cursor{blocks};
        // Braced initialization advances the cursor in physical column order.
        return Result{cursor.column_pointer<Ts>(data)...};
    }
    static_assert(max_capacity >= capacity_granularity);
};

constexpr auto try_round_capacity(std::int64_t const required,
                                  std::uint32_t const maximum,
                                  std::uint32_t& result) noexcept -> bool {
    if (required < 0 || required > maximum) {
        return false;
    }
    auto const rounded{((required + capacity_granularity - 1) / capacity_granularity) *
                       capacity_granularity};
    if (rounded > maximum) {
        return false;
    }
    result = static_cast<std::uint32_t>(rounded);
    return true;
}

constexpr auto try_allocation_bytes(std::uint32_t const capacity,
                                    std::size_t const block_bytes,
                                    std::size_t& result) noexcept -> bool {
    if (capacity % capacity_granularity != 0 || capacity > maximum_capacity(block_bytes)) {
        return false;
    }
    result = static_cast<std::size_t>(capacity / capacity_granularity) * block_bytes;
    return true;
}

}
