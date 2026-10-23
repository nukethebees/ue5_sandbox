#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace lispb::codegen {

struct EnvironmentId {
    inline static constexpr auto null_value{std::numeric_limits<std::uint16_t>::max()};
    std::uint16_t value{null_value};

    constexpr auto is_null() const -> bool { return value == null_value; }
    auto operator<=>(EnvironmentId const&) const = default;
};

}
