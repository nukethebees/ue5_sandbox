#pragma once

#include <compare>
#include <cstdint>

namespace ioj::sim {

struct IndexSpan {
    std::int32_t offset{0};
    std::int32_t count{0};

    auto operator<=>(IndexSpan const&) const noexcept = default;

    [[nodiscard]] constexpr auto is_empty() const noexcept -> bool { return count == 0; }
    [[nodiscard]] constexpr auto start() const noexcept -> std::int32_t { return offset; }
    [[nodiscard]] constexpr auto end() const noexcept -> std::int32_t { return offset + count; }
};
} // namespace ioj::sim
