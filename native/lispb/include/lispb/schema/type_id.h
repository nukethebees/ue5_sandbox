#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace lispb::schema {
struct TypeId {
    inline static constexpr std::uint32_t invalid_value{std::numeric_limits<std::uint32_t>::max()};

    std::uint32_t value{invalid_value};

    [[nodiscard]] auto valid() const -> bool { return value != invalid_value; }
    auto operator<=>(TypeId const&) const = default;
};
}
