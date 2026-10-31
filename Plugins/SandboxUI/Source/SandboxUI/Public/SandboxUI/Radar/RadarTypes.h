#pragma once

#include <ioj/ui/radar.h>

#include "Containers/Array.h"
#include "Math/Color.h"
#include "Math/Vector.h"

#include <cstddef>
#include <type_traits>

struct SANDBOXUI_API FRadarFrame {
    TArray<ioj::ui::radar::Instance> instances;
    float combat_display_radius{0.45f};
    float tactical_display_radius{0.8f};
};

struct SANDBOXUI_API FRadarStyle {
    FLinearColor structure_color{1.0f, 0.7f, 0.12f, 1.0f};
    FLinearColor objective_color{1.0f, 0.78f, 0.16f, 1.0f};
    FLinearColor selection_color{1.0f, 0.94f, 0.72f, 1.0f};
    FLinearColor player_color{1.0f, 0.78f, 0.16f, 1.0f};
    FLinearColor plane_color{FLinearColor::Transparent};
    float structure_opacity{0.82f};
    float grid_opacity{0.55f};
    float core_cell_radius{0.18f};
    float combat_cell_radius{0.075f};
    float tactical_cell_radius{0.045f};
    float strategic_cell_radius{0.025f};
    float stem_opacity{0.42f};
    float glyph_intensity{1.0f};
    float glyph_size_scale{1.0f};
    float objective_size_multiplier{1.2f};
    float objective_ring_padding_pixels{3.0f};
    float objective_ring_thickness_pixels{1.25f};
    float contact_glow_opacity{0.0f};
    float emphasized_glow_opacity{0.06f};
};

[[nodiscard]] SANDBOXUI_API auto pack_radar_color(FLinearColor color) -> uint32;
