#pragma once

#include <compare>
#include <cstdint>

namespace lispb::codegen {

template <typename Id>
struct SchemaRange {
    std::uint32_t offset{};
    std::uint32_t count{};
    [[nodiscard]] auto empty() const -> bool { return count == 0; }
    [[nodiscard]] auto size() const -> std::uint32_t { return count; }
    auto operator<=>(SchemaRange const&) const = default;
};

} // namespace lispb::codegen
