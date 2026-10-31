#pragma once

#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/entity_telemetry.h>
#include <ioj/sim/entities/ship_health.h>
#include <ioj/sim/player/fire_rate.h>
#include <ioj/sim/player/flight_model_enums.h>
#include <ioj/sim/player/space_ship_common.h>
#include <ioj/vector.h>

#include <cstdint>
#include <string>
#include <vector>

namespace ioj::presentation {
struct MissionHudData {
    auto operator==(MissionHudData const&) const -> bool = default;

    std::string active_titles;
    std::int32_t active{};
    std::int32_t completed{};
    std::int32_t inactive{};
    double elapsed_seconds{};
};

struct EntityCountHudData {
    auto operator==(EntityCountHudData const&) const -> bool = default;

    sim::telemetry::EntityCounts alive_per_team_and_type{};
};

struct TopKillerEntry {
    auto operator==(TopKillerEntry const&) const -> bool = default;

    sim::EntityId entity_id{};
    sim::EntityType entity_type{};
    Team team{};
    std::int32_t kills{};
};

struct KillHudData {
    auto operator==(KillHudData const&) const -> bool = default;

    std::vector<TopKillerEntry> top_killers;
    sim::telemetry::EntityCounts team_kill_matrix{};
};

struct PlayerStatusHudData {
    auto operator==(PlayerStatusHudData const&) const -> bool = default;

    bool has_player_ship{};
    sim::ShipHealth health{};
    float speed{};
    float target_speed{};
    float energy{1.f};
    std::int32_t points{};
    sim::ShipFireRate fire_rate{sim::ShipFireRate::Single};
};

struct FlightVectorDebugData {
    auto operator==(FlightVectorDebugData const&) const -> bool = default;

    Vector2f turn_input{};
    Vector2f move_input{};
    Vector2f target_velocity{};
    Vector2f local_velocity{};
};

struct PlayerFlightHudData {
    auto operator==(PlayerFlightHudData const&) const -> bool = default;

    bool has_player_ship{};
    FlightVectorDebugData flight_vector_debug{};
    Vector3d ship_velocity{};
    Vector3d target_velocity{};
    sim::player::FlightModelPreset flight_model_preset{};
    bool flight_model_customized{};
    float throttle{};
    sim::player::BoostBrakeState boost_brake_state{};
    Vector3d crosshair_origin{};
    Vector3d crosshair_direction{};
    std::string selected_mapping_context;
};

struct SampledSpeedHudData {
    auto operator==(SampledSpeedHudData const&) const -> bool = default;

    std::vector<Vector2d> samples;
    std::uint32_t oldest_index{};
};

struct HudChanges {
    bool mission{};
    bool entity_counts{};
    bool player_status{};
    bool player_flight{};
    bool sampled_speed{};
};

}
