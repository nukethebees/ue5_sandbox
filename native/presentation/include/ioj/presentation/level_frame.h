#pragma once

#include <ioj/presentation/agent_display_batch.h>
#include <ioj/presentation/player_flight_hud_data.h>
#include <ioj/presentation/player_status_hud_data.h>
#include <ioj/presentation/sampled_speed_hud_view.h>
#include <ioj/rotators3f_view.h>
#include <ioj/sim/collision/world_aabbs_view.h>
#include <ioj/sim/entities/entity_frame_change.h>
#include <ioj/sim/entities/team_entity_array.h>
#include <ioj/sim/lasers/laser_hit_details_view.h>
#include <ioj/sim/lasers/laser_source.h>
#include <ioj/sim/levels/frame_event_counts.h>
#include <ioj/sim/levels/level_state.h>
#include <ioj/sim/player/player_event_kind.h>
#include <ioj/sim/sim_tick.h>
#include <ioj/transform3d.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace ioj::presentation {
struct PlayerFrame {
    sim::EntityId id;
    Team team;
    Transform3d transform;
    Transform3d body_transform;
    Transform3d middle_socket;
    float weapon_range;
    PlayerStatusHudData status;
    PlayerFlightHudData flight;
    SampledSpeedHudView sampled_speed;
};

struct TargetView {
    std::span<sim::EntityId const> ids;
    vectors3f::ConstView locations;
};

struct LaserView {
    vectors3f::ConstView locations;
    rotators3f::ConstView rotations;
    std::span<std::uint8_t const> active;
    std::span<sim::LaserSource const> sources;
    std::span<float const> initial_lifetimes;
    std::span<float const> spawn_times;
};

struct InstanceChanges {
    std::span<sim::EntityFrameChangeKind const> kinds;
    std::span<std::uint32_t const> indices;
    std::span<Vector3f const> locations;
    std::span<Rotator3f const> rotations;
    std::span<Team const> teams;
};

struct FrameEvents {
    sim::SimTick base_tick;
    std::span<sim::FrameEventCounts const> ticks;
    std::span<sim::player::PlayerEventKind const> player;
    std::span<sim::EntityId const> victims;
    std::span<Vector3f const> death_locations;
    sim::laser_hit_details::ConstView impacts;
    InstanceChanges capital_changes;
    InstanceChanges turret_changes;
};

struct LevelStatus {
    sim::SimTick completed_tick{};
    sim::LevelState state{};
    std::optional<sim::SimTick::value_type> ticks_remaining{};
};

// All borrowed columns expire on return from the presentation call.
struct LevelFrame {
    auto const& capitals() const { return agents[0]; }
    auto const& fighters() const { return agents[1]; }
    auto const& turrets() const { return agents[2]; }
    auto const& spinners() const { return agents[3]; }

    LevelStatus status;
    std::array<AgentDisplayBatch, 4> agents;
    rotators3f::ConstView capital_rotations;
    vectors3f::ConstView fighter_aim;
    TargetView fighter_targets;
    rotators3f::ConstView turret_rotations;
    TargetView turret_targets;
    std::span<float const> spinner_yaws;
    LaserView lasers;
    sim::collision::world_aabbs::ConstView entity_bounds;
    sim::EntityCounts const& entity_counts;
    std::optional<PlayerFrame> player{};
    FrameEvents events;
};
}
