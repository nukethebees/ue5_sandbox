#pragma once

#include "Containers/Array.h"
#include "Math/Color.h"
#include "Math/Vector.h"
#include "Misc/EnumClassFlags.h"

#include <cstddef>
#include <type_traits>

enum class ERadarGlyph : uint8 {
    Player,
    Fighter,
    CapitalShip,
    Turret,
    Missile,
    Waypoint,
    Unknown,
};

enum class ERadarContactFlags : uint8 {
    None = 0,
    Selected = 1 << 0,
    DefendObjective = 1 << 1,
    DestroyObjective = 1 << 2,
};
ENUM_CLASS_FLAGS(ERadarContactFlags);

struct SANDBOXUI_API FRadarInstance {
    FVector3f radar_position{FVector3f::ZeroVector};
    float size_scale{1.0f};
    uint32 packed_color{0xffffffffu};
    uint32 packed_glyph_and_flags{0};
};

static_assert(sizeof(FRadarInstance) == 24);
static_assert(std::is_standard_layout_v<FRadarInstance>);
static_assert(std::is_trivially_copyable_v<FRadarInstance>);
static_assert(offsetof(FRadarInstance, radar_position) == 0);
static_assert(offsetof(FRadarInstance, size_scale) == 12);
static_assert(offsetof(FRadarInstance, packed_color) == 16);
static_assert(offsetof(FRadarInstance, packed_glyph_and_flags) == 20);

struct SANDBOXUI_API FRadarFrame {
    TArray<FRadarInstance> instances;
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
    float grid_opacity{0.14f};
    float stem_opacity{0.42f};
    float glyph_intensity{1.0f};
    float contact_glow_opacity{0.12f};
    float emphasized_glow_opacity{0.35f};
};

[[nodiscard]] SANDBOXUI_API auto pack_radar_color(FLinearColor color) -> uint32;
[[nodiscard]] SANDBOXUI_API auto pack_radar_display(ERadarGlyph glyph, ERadarContactFlags flags)
    -> uint32;
