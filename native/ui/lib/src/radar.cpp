#include <ioj/ui/radar.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ioj::ui::radar {
auto pack_color(Color4f const color) noexcept -> std::uint32_t {
    auto const channel = [](float const value) {
        return static_cast<std::uint32_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    };
    return channel(color.r) | (channel(color.g) << 8u) | (channel(color.b) << 16u) |
           (channel(color.a) << 24u);
}

auto pack_display(Glyph const glyph, ContactFlags const flags, float const heading_radians) noexcept
    -> std::uint32_t {
    constexpr float full_turn{2.0f * std::numbers::pi_v<float>};
    auto heading{std::fmod(heading_radians, full_turn)};
    if (heading < 0.0f) {
        heading += full_turn;
    }
    auto const packed_heading{
        static_cast<std::uint32_t>(std::lround(heading * (65535.0f / full_turn)))};
    return static_cast<std::uint32_t>(glyph) | (static_cast<std::uint32_t>(flags) << 8u) |
           ((packed_heading & 0xffffu) << 16u);
}
}
