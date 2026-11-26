#pragma once

#include <ioj/gfx/starfield_settings.h>
#include <ioj/levels/control_context.h>
#include <ioj/levels/launch_mode.h>
#include <ioj/levels/presentation_mode.h>
#include <ioj/levels/team.h>
#include <ioj/presentation/entity_overlay_health_maximums.h>
#include <ioj/rotator_types.h>
#include <ioj/settings/presentation.h>
#include <ioj/sim/collision/collision_grid_spec.h>
#include <ioj/sim/collision/world_aabbs_view.h>
#include <ioj/sim/entities/entity_type_radii.h>
#include <ioj/sim/levels/hud_settings.h>
#include <ioj/transform3d.h>

#include <optional>
#include <string_view>

namespace ioj::presentation {
struct PlayerSpawn {
    Team team;
    Vector3d position;
    Rotator3d rotation;
    std::optional<std::int32_t> initial_health;
};

struct MissionInit {
    std::string_view id;
    std::string_view source_path;
    levels::ControlContext control_context;
    levels::PresentationMode presentation_mode;
    levels::LaunchMode launch_mode;
    settings::Presentation const& presentation;
    sim::HudSettings const& hud;
    starfield::Settings const& starfield;
    EntityOverlayHealthMaximums health_maximums;
    std::int32_t laser_reserve;
    sim::collision::CollisionGridSpec grid;
    sim::EntityTypeRadii radii;
    sim::collision::world_aabbs::ConstView static_bounds;
    std::optional<PlayerSpawn> player{};
    std::optional<Transform3d> camera{};
};
}
