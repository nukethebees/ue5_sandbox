#pragma once

#include <lispb/schema/packed_segment_kind.h>

#include <compare>
#include <cstdint>

namespace lispb::schema {

struct PackedSegmentId {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{};
    [[nodiscard]] constexpr auto kind() const -> PackedSegmentKind {
        return static_cast<PackedSegmentKind>(value >> kind_shift);
    }
    [[nodiscard]] constexpr auto index() const -> std::uint32_t { return value & index_mask; }
    auto operator<=>(PackedSegmentId const&) const = default;
};
static_assert(sizeof(PackedSegmentId) == 4);

} // namespace lispb::schema
