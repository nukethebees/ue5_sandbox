#pragma once

#include <lispb/target_kind.h>

#include <compare>
#include <cstdint>
namespace lispb {

struct TargetId {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};

    std::uint32_t value{};
    [[nodiscard]] auto kind() const -> TargetKind {
        return static_cast<TargetKind>(value >> kind_shift);
    }
    [[nodiscard]] auto index() const -> std::uint32_t { return value & index_mask; }
    auto operator<=>(TargetId const&) const = default;
};

}
