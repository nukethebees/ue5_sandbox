#pragma once

#include <lispb/schema/packed_segment_kind.h>

#include <compare>
#include <cstdint>

namespace lispb::schema {

struct PackedSegmentId {
    std::uint32_t value{};
    [[nodiscard]] constexpr auto kind() const -> PackedSegmentKind {
        return static_cast<PackedSegmentKind>(value >> 24);
    }
    [[nodiscard]] constexpr auto index() const -> std::uint32_t { return value & 0xffffffu; }
    auto operator<=>(PackedSegmentId const&) const = default;
};
static_assert(sizeof(PackedSegmentId) == 4);

} // namespace lispb::schema
