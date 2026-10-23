#pragma once

#include <codegen/schema/packed_segment_kind.h>

#include <cassert>
#include <compare>
#include <cstdint>

namespace lispb::codegen {

struct PackedSegmentId {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{};
    [[nodiscard]] auto kind() const -> PackedSegmentKind {
        return static_cast<PackedSegmentKind>(value >> kind_shift);
    }
    [[nodiscard]] auto index() const -> std::uint32_t {
        assert((value & index_mask) != 0);
        return (value & index_mask) - 1;
    }
    auto operator<=>(PackedSegmentId const&) const = default;
};

} // namespace lispb::codegen
