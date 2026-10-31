#pragma once

#include <ioj/ui/types.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ioj::ui::radar {
enum class Glyph : std::uint8_t {
    Player,
    Fighter,
    CapitalShip,
    Turret,
    Missile,
    Waypoint,
    Unknown,
};

enum class ContactFlags : std::uint8_t {
    None = 0,
    Selected = 1 << 0,
    DefendObjective = 1 << 1,
    DestroyObjective = 1 << 2,
};

constexpr auto operator|(ContactFlags const lhs, ContactFlags const rhs) -> ContactFlags {
    return static_cast<ContactFlags>(static_cast<std::uint8_t>(lhs) |
                                     static_cast<std::uint8_t>(rhs));
}

struct Instance {
    Vector3f radar_position{};
    float size_scale{1.0f};
    std::uint32_t packed_color{0xffffffffu};
    std::uint32_t packed_glyph_and_flags{};
};

static_assert(sizeof(Instance) == 24);
static_assert(std::is_standard_layout_v<Instance>);
static_assert(std::is_trivially_copyable_v<Instance>);
static_assert(offsetof(Instance, radar_position) == 0);
static_assert(offsetof(Instance, size_scale) == 12);
static_assert(offsetof(Instance, packed_color) == 16);
static_assert(offsetof(Instance, packed_glyph_and_flags) == 20);

auto pack_color(Color4f color) noexcept -> std::uint32_t;
auto pack_display(Glyph glyph, ContactFlags flags, float heading_radians = 0.0f) noexcept
    -> std::uint32_t;
}
