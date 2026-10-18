#pragma once

#include <lispb/target_kind.h>

#include <compare>
#include <cstdint>
namespace lispb {

struct TargetId {
    std::uint32_t value{};
    [[nodiscard]] auto kind() const -> TargetKind { return static_cast<TargetKind>(value >> 24); }
    [[nodiscard]] auto index() const -> std::uint32_t { return value & 0x00ffffffu; }
    auto operator<=>(TargetId const&) const = default;
};

}
