#include "SandboxUI/Radar/RadarTypes.h"

auto pack_radar_color(FLinearColor const color) -> uint32 {
    return ioj::ui::radar::pack_color({color.R, color.G, color.B, color.A});
}
