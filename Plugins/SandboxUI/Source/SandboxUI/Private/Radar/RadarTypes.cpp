#include "SandboxUI/Radar/RadarTypes.h"

auto pack_radar_color(FLinearColor const color) -> uint32 {
    auto const quantized{color.ToFColor(false)};
    return static_cast<uint32>(quantized.R) | (static_cast<uint32>(quantized.G) << 8u) |
           (static_cast<uint32>(quantized.B) << 16u) | (static_cast<uint32>(quantized.A) << 24u);
}

auto pack_radar_display(ERadarGlyph const glyph,
                        ERadarContactFlags const flags,
                        float const heading_radians) -> uint32 {
    auto normalized_heading{FMath::Fmod(heading_radians, UE_TWO_PI)};
    if (normalized_heading < 0.0f) {
        normalized_heading += UE_TWO_PI;
    }
    auto const packed_heading{
        static_cast<uint32>(FMath::RoundToInt(normalized_heading * (65535.0f / UE_TWO_PI)))};
    return static_cast<uint32>(glyph) | (static_cast<uint32>(flags) << 8u) |
           ((packed_heading & 0xffffu) << 16u);
}
