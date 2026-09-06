#include "SandboxUI/Radar/RadarTypes.h"

auto pack_radar_color(FLinearColor const color) -> uint32 {
    return color.ToFColor(false).DWColor();
}

auto pack_radar_display(ERadarGlyph const glyph, ERadarContactFlags const flags) -> uint32 {
    return static_cast<uint32>(glyph) | (static_cast<uint32>(flags) << 8u);
}
