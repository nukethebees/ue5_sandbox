#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>

namespace ml::single_allocation_layout {

inline constexpr std::int32_t capacity_granularity{64};

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
