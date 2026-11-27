#pragma once

#include <ioj/gfx/starfield_settings.h>
#include <ioj/levels/control_context.h>
#include <ioj/levels/launch_mode.h>
#include <ioj/levels/presentation_mode.h>
#include <ioj/presentation/column_views.h>
#include <ioj/presentation/entity_overlay_health_maximums.h>
#include <ioj/settings/presentation.h>
#include <ioj/sim/collision/collision_grid_spec.h>
#include <ioj/sim/entities/entity_type_radii.h>
#include <ioj/sim/levels/hud_settings.h>
#include <ioj/transform3d.h>

#include <optional>
#include <string>
#include <string_view>

namespace ioj::presentation {
struct LevelPresentationSetup {
    std::string source_path;
    levels::ControlContext control_context;
    levels::PresentationMode presentation_mode;
    levels::LaunchMode launch_mode;
    settings::Presentation presentation;
    sim::HudSettings hud;
    starfield::Settings starfield;
    EntityOverlayHealthMaximums health_maximums;
    std::int32_t laser_reserve;
    std::optional<Transform3d> camera{};
};

struct LevelInit {
    LevelPresentationSetup const& setup;
    std::string_view id;
    sim::collision::CollisionGridSpec grid;
    sim::EntityTypeRadii radii;
    world_aabbs::ConstView static_bounds;
};
}
