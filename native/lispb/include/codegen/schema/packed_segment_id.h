#pragma once

#include <codegen/schema/packed_segment_kind.h>

#include <cassert>
#include <compare>
#include <cstdint>

namespace lispb::codegen {

struct PackedSegmentId {
    std::uint32_t value{};
    [[nodiscard]] auto kind() const -> PackedSegmentKind {
        return static_cast<PackedSegmentKind>(value >> 24);
    }
    [[nodiscard]] auto index() const -> std::uint32_t {
        assert((value & 0x00ffffff) != 0);
        return (value & 0x00ffffff) - 1;
    }
    auto operator<=>(PackedSegmentId const&) const = default;
};

} // namespace lispb::codegen
