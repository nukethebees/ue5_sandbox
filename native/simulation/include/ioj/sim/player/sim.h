#include <ioj/sim/entities/entity_tables.h>
#pragma once
#include <ioj/frame_memory_resource.h>
#include <ioj/kill_count.h>
#include <ioj/levels/team.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/health_table.h>
#include <ioj/sim/entities/ship_health.h>
#include <ioj/sim/player/flight_controller_state.h>
#include <ioj/sim/player/laser_firing_state.h>
#include <ioj/sim/player/phase_inputs.h>
#include <ioj/sim/player/physical_movement_state.h>
#include <ioj/sim/player/player_events.h>
#include <ioj/sim/player/player_flight_intent.h>
#include <ioj/sim/player/player_presentation_state.h>
#include <ioj/sim/player/player_resource_state.h>
#include <ioj/sim/player/player_sim_config.h>
#include <ioj/sim/player/player_simulation_state.h>
#include <ioj/sim/player/ship_laser_mode.h>
#include <ioj/sim/player/space_ship_common.h>
#include <ioj/sim/player/trigger_mode.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/transform3d.h>
#include <ioj/vector_value_math.h>

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::sim {
struct LevelSim;
class EntityLedger;
struct PlayerSimTestAccess;
}

namespace ioj::sim::player {
class InitialisationInterface;
class PreparationInterface;
class ThinkingInterface;
class ActionInterface;
class ResolutionInterface;

struct PlayerSpawnData {
    PlayerSimConfig config;
    bool sample_speed{};
    Team team{Team::White};

    Transform3d transform{Transform3d{}};
    Transform3d body_transform{Transform3d{}};
    Transform3d left_socket{Transform3d{}};
    Transform3d right_socket{Transform3d{}};
    Transform3d middle_socket{Transform3d{}};

    ShipLaserMode laser_mode{ShipLaserMode::Single};
    ShipTriggerMode laser_trigger_mode{ShipTriggerMode::Burst3};

    ShipHealth health{Health{1000}};
};

// Simulate the controlled ship from player commands.
struct Sim {
    /* **************************************** */
    // Construction and configuration
    /* **************************************** */
    Sim(std::pmr::memory_resource& memory);
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    auto physical_state() const noexcept -> PhysicalMovementState const& { return state_.physical; }
    auto controller_state() const noexcept -> FlightControllerState const& {
        return state_.controller;
    }

    auto resource_state() const noexcept -> PlayerResourceState const& { return state_.resources; }
    auto presentation_state() const noexcept -> PlayerPresentationState const& {
        return state_.presentation;
    }

    auto flight_intent() const noexcept -> PlayerFlightIntent const& { return flight_intent_; }
    auto target_speed() const noexcept -> float;
    void set_config(PlayerSimConfig const& new_config) noexcept;
    void set_team(Team new_team, HealthTable const& health_table, EntityLedger& ledger) noexcept;

    /* **************************************** */
    // Weapons
    /* **************************************** */
    void upgrade_laser() noexcept;
    void select_next_laser_trigger_mode() noexcept;
    void select_previous_laser_trigger_mode() noexcept;
    void set_laser_trigger_mode(ShipTriggerMode value) noexcept;

    /* **************************************** */
    // Health and status
    /* **************************************** */
    void add_health(Health delta);
    [[nodiscard]] auto health(HealthTable const& health_table) const -> ShipHealth;
    [[nodiscard]] auto is_alive(HealthTable const& health_table) const -> bool;

    auto speed() const noexcept -> float;
    auto energy() const -> float;
    auto energy_is_full() const -> bool;
    auto get_middle_socket() const -> Transform3d;
    auto laser_effective_range() const noexcept -> float;

    /* **************************************** */
    // Sim state
    /* **************************************** */
    EntityId unique_entity_id;
    Team team{Team::White};

    Transform3d left_socket{Transform3d{}};
    Transform3d right_socket{Transform3d{}};
    Transform3d middle_socket{Transform3d{}};

    ShipLaserMode laser_mode{ShipLaserMode::Single};
    float laser_shot_cooldown{0.f};
    std::int32_t lasers_fired_this_burst{0};
    std::int32_t lasers_per_burst{3};
    LaserFiringState laser_firing_mode{LaserFiringState::idle};
    ShipTriggerMode laser_trigger_mode{ShipTriggerMode::Burst3};

    std::uint32_t speed_sample_index{0};
    std::uint32_t speed_sample_max{0};
    std::int32_t speed_sample_ticks_remaining{0};
    std::int32_t speed_sample_tick_period{1};
    std::vector<Vector2d> speed_samples;
  private:
    /* **************************************** */
    // Tick phases
    /* **************************************** */
    void initialise(PlayerSpawnData const& spawn,
                    EntityTables& tables,
                    EntityLedger& ledger,
                    SimClock const& clock);
    void update_entity_lookup_table(EntityTables& tables, [[maybe_unused]] SimClock const& clock);
    void prepare_tick(float dt, EntityTables& tables, SimClock const& clock);
    void think(float dt, PlayerEvents& events);
    void apply_movement(ActionInputs const inputs);
    void generate_fire_commands();
    void resolve_damage_events(ResolutionInputs const inputs);

    /* **************************************** */
    // Movement
    /* **************************************** */
    void reset_speed_responses(PlayerSimulationState& state) const noexcept;
    void update_movement(float dt, PlayerSimulationState& state) const noexcept;
    void update_body_orientation(float dt, PlayerSimulationState& state);
    void update_boost_brake(float dt, PlayerSimulationState& state, PlayerEvents& events);
    void update_propulsion_action(PlayerSimulationState& state, PlayerEvents& events);

    /* **************************************** */
    // Weapons
    /* **************************************** */
    void set_laser_mode(LaserFiringState mode) noexcept;
    void update_laser_firing();
    void fire_laser();
    void materialize_fire_command(ActionInputs const inputs);
    void fire_lasers_from(ActionInputs const inputs, std::span<Transform3d const> fire_points);

    /* **************************************** */
    // Health
    /* **************************************** */
    void apply_inputs(PlayerSimulationState& state, PlayerEvents& events);

    /* **************************************** */
    // Diagnostics
    /* **************************************** */
    void sample_speed(SimClock const& clock);
    void configure_speed_sampling(SimClock const& clock);

    /* **************************************** */
    // Dependencies and internal state
    /* **************************************** */
    friend class InitialisationInterface;
    friend class PreparationInterface;
    friend class ThinkingInterface;
    friend class ActionInterface;
    friend class ResolutionInterface;
    friend struct sim::LevelSim;
    friend struct sim::PlayerSimTestAccess;

    PlayerSimulationState state_{};
    PlayerSimulationState planned_state_{};
    PlayerFlightIntent flight_intent_{};
    bool fire_requested_{};
    PlayerSimConfig config_{};
    Health max_health_{1000};
    PlayerInputEvents input_queue_;
    std::pmr::vector<Health> health_changes_;
};
} // namespace ioj::sim::player
