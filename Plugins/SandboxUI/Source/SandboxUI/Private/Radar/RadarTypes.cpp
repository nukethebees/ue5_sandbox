#include "SandboxUI/Radar/RadarTypes.h"

auto pack_radar_color(FLinearColor const color) -> uint32 {
    auto const quantized{color.ToFColor(false)};
    return static_cast<uint32>(quantized.R) | (static_cast<uint32>(quantized.G) << 8u) |
           (static_cast<uint32>(quantized.B) << 16u) | (static_cast<uint32>(quantized.A) << 24u);
}

auto pack_radar_display(ERadarGlyph const glyph, ERadarContactFlags const flags) -> uint32 {
    return static_cast<uint32>(glyph) | (static_cast<uint32>(flags) << 8u);
}
