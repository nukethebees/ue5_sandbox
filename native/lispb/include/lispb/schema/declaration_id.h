#pragma once

#include <compare>
#include <cstdint>

namespace lispb::schema {
struct DeclarationId {
    inline static constexpr std::uint64_t invalid_value{0};

    std::uint64_t value{invalid_value};

    [[nodiscard]] auto valid() const -> bool { return value != invalid_value; }
    auto operator<=>(DeclarationId const&) const = default;
};
}
