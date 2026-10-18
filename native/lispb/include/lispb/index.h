#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace lispb {

// DefaultValue preserves the owning store's empty/default index convention.
template <typename Tag, std::uint32_t DefaultValue = 0>
struct Index {
    std::uint32_t value{DefaultValue};
    constexpr auto operator<=>(Index const&) const = default;
};

static_assert(sizeof(Index<void>) == 4);

}

template <typename Tag, std::uint32_t DefaultValue>
struct std::hash<lispb::Index<Tag, DefaultValue>> {
    constexpr auto operator()(lispb::Index<Tag, DefaultValue> const index) const noexcept
        -> std::size_t {
        return index.value;
    }
};
