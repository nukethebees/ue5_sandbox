#pragma once

#include <cstdint>

namespace ioj {
struct ScreenResolution {
    auto operator==(ScreenResolution const&) const -> bool = default;

    std::int32_t width{};
    std::int32_t height{};
};

enum class GameWindowMode : std::uint8_t {
    Windowed,
    Borderless,
    Fullscreen,
};

enum class GameAntiAliasingMethod : std::uint8_t {
    Off,
    FXAA,
    TAA,
    TSR,
    SMAA,
};

enum class GameQualityLevel : std::uint8_t {
    Low,
    Medium,
    High,
    Epic,
};

enum class GameGraphicsPreset : std::uint8_t {
    Custom,
    Low,
    Medium,
    High,
    Epic,
};

} // namespace ioj
