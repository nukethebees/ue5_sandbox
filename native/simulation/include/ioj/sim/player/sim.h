#pragma once
#include <cstdint>
#include <ioj/sim/health_table.h>
#include <ioj/sim/player/player_read_view.h>
#include <ioj/sim/ship_health.h>
#include <ioj/sim/transform3d.h>
#include <sandbox/core/vector2d.h>
#include <span>
#include <vector>

#include <ioj/sim/sim_config.h>

#include <ioj/sim/entity_types.h>
#include <ioj/sim/player/fire_rate.h>
#include <ioj/sim/player/flight_model_runtime.h>
#include <ioj/sim/player/laser_firing_state.h>
#include <ioj/sim/player/ship_laser_mode.h>
#include <ioj/sim/player/space_ship_common.h>
#include <ioj/sim/sim_clock.h>

namespace ioj::sim {
struct LevelSim;
class EntityLedger;
class CombatEvents;
struct SpatialQueryManager;
struct PlayerSimTestAccess;
}

namespace ioj::sim::lasers {
struct Sim;
}

namespace ioj::sim::player {
class PhaseInterface;

struct PlayerSpawnData {
    PlayerSimConfig config;
    Team team{Team::White};

    Transform3d transform{Transform3d{}};
    Transform3d body_transform{Transform3d{}};
    Transform3d left_socket{Transform3d{}};
    Transform3d right_socket{Transform3d{}};
    Transform3d middle_socket{Transform3d{}};

    FlightModelLoadout flight_models{make_default_flight_model_loadout()};

    ShipLaserMode laser_mode{ShipLaserMode::Single};
    ShipFireRate laser_fire_rate{ShipFireRate::Burst3};

    ShipHealth health{1000};
};

struct Sim {
    /* **************************************** */
    // Construction and configuration
    /* **************************************** */
    Sim(SimClock const& clock,
        EntityLedger& ledger,
        CombatEvents const& combat_events,
        HealthTable& health_table,
        SpatialQueryManager const& spatial_query_manager,
        lasers::Sim& lasers);
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    auto get_read_view() const -> PlayerReadView {
        return {state_.physical.transform,
                state_.presentation.body_transform,
                get_middle_socket(),
                state_.physical.velocity,
                state_.controller.effective_action,
                laser_firing_mode,
                state_.presentation.boost_start_sequence};
    }
    auto get_physical_state() const noexcept -> PhysicalMovementState const& {
        return state_.physical;
    }
    auto get_controller_state() const noexcept -> FlightModelControllerState const& {
        return state_.controller;
    }
    auto get_resource_state() const noexcept -> PlayerResourceState const& {
        return state_.resources;
    }
    auto get_presentation_state() const noexcept -> PlayerPresentationState const& {
        return state_.presentation;
    }
    auto get_flight_intent() const noexcept -> PlayerFlightIntent const& { return flight_intent_; }
    auto get_active_flight_model_slot() const noexcept -> FlightModelSlot {
        return active_flight_model_slot_;
    }
    auto get_active_flight_model_profile() const noexcept -> FlightModelProfile const& {
        return flight_model_profile(flight_models_, active_flight_model_slot_);
    }
    auto get_active_flight_model_config() const noexcept -> FlightModelConfig const& {
        return get_active_flight_model_profile().config;
    }
    void configure(PlayerSpawnData const& spawn) noexcept;
    void set_config(PlayerSimConfig const& new_config) noexcept;
    void set_team(Team new_team) noexcept;
    void set_speed_sampling_enabled(bool enabled) noexcept { speed_sampling_enabled = enabled; }

    /* **************************************** */
    // Flight controls
    /* **************************************** */
    void set_forward_input(float input) noexcept;
    void set_right_input(float input) noexcept;
    void set_up_input(float input) noexcept;
    void set_ship_2d_control(ml::Vector2d input);
    void start_sampling() noexcept;
    void stop_sampling();
    void adjust_desired_forward_velocity(float direction);
    void set_pitch_input(float input) noexcept;
    void set_yaw_input(float input) noexcept;
    void set_roll_input(float input) noexcept;
    void set_accelerator(float input) noexcept;
    void start_boost();
    void stop_boost();
    void start_brake();
    void start_emergency_brake();
    void stop_emergency_brake();
    void stop_brake();
    void select_flight_model_slot(FlightModelSlot slot) noexcept;
    [[nodiscard]] auto set_flight_model_slot_profile(FlightModelSlot slot,
                                                     FlightModelProfile profile) noexcept -> bool;

    /* **************************************** */
    // Weapons
    /* **************************************** */
    void start_fire_laser();
    void stop_fire_laser();
    void upgrade_laser() noexcept;
    void select_next_laser_fire_rate() noexcept;
    void select_previous_laser_fire_rate() noexcept;
    void set_laser_fire_rate(ShipFireRate value) noexcept;

    /* **************************************** */
    // Health and status
    /* **************************************** */
    void add_health(Health added_health);
    [[nodiscard]] auto get_health() const -> ShipHealth;
    [[nodiscard]] auto get_health_index() const noexcept -> HealthIndex { return health_index_; }
    [[nodiscard]] auto is_alive() const -> bool;
    auto consume_death_notification() noexcept -> bool;

    auto get_kills() const -> std::int32_t;
    auto get_speed() const noexcept -> float;
    auto is_sampling_target_speed() const noexcept -> bool { return sampling_target_speed_; }
    auto get_sampled_target_speed_scale() const noexcept -> ml::Vector2d {
        return sampled_target_speed_scale_;
    }
    auto get_energy() const -> float;
    auto energy_is_full() const -> bool;
    auto get_middle_socket() const -> Transform3d;
    auto get_laser_effective_range() const noexcept -> float;

    /* **************************************** */
    // Sim state
    /* **************************************** */
    EntityUniqueId unique_entity_id;
    Team team{Team::White};

    Transform3d left_socket{Transform3d{}};
    Transform3d right_socket{Transform3d{}};
    Transform3d middle_socket{Transform3d{}};

    ShipLaserMode laser_mode{ShipLaserMode::Single};
    float laser_shot_cooldown{0.f};
    std::int32_t lasers_fired_this_burst{0};
    std::int32_t lasers_per_burst{3};
    EntityUniqueId lock_on_target{};
    LaserFiringState laser_firing_mode{LaserFiringState::idle};
    ShipFireRate laser_fire_rate{ShipFireRate::Burst3};

    bool speed_sampling_enabled{};

    std::int32_t speed_sample_index{0};
    std::int32_t speed_sample_max{0};
    std::int32_t speed_sample_ticks_remaining{0};
    std::int32_t speed_sample_tick_period{1};
    std::vector<ml::Vector2d> speed_samples;
  private:
    PlayerSimulationState state_{};
    PlayerSimulationState planned_state_{};
    PlayerFlightIntent flight_intent_{};
    ml::Vector2d sampled_target_speed_scale_{};
    bool sampling_target_speed_{};
    FlightModelLoadout flight_models_{make_default_flight_model_loadout()};
    FlightModelSlot active_flight_model_slot_{FlightModelSlot::Up};
    /* **************************************** */
    // Tick phases
    /* **************************************** */
    void begin_play();
    void prepare_tick(float dt);
    void think(float dt);
    void apply_movement();
    void generate_fire_commands();
    void resolve_damage_events();

    /* **************************************** */
    // Identity and accounting
    /* **************************************** */

    /* **************************************** */
    // Movement
    /* **************************************** */
    void update_body_orientation(float dt, PlayerSimulationState& state);
    void update_boost_brake(float dt, PlayerSimulationState& state);
    void refresh_effective_action(PlayerSimulationState& state) noexcept;

    /* **************************************** */
    // Weapons
    /* **************************************** */
    void set_lock_on_target(EntityUniqueId target) noexcept;
    void set_laser_mode(LaserFiringState mode) noexcept;
    void update_laser_firing();
    void fire_laser();
    void materialize_fire_command();
    bool fire_requested_{};
    void fire_lasers_from(std::span<Transform3d const> fire_points);

    /* **************************************** */
    // Health
    /* **************************************** */
    void set_health(Health new_health, EntityUniqueId killer = {});
    auto health_ref() -> Health&;
    void die(EntityUniqueId killer);

    /* **************************************** */
    // Diagnostics
    /* **************************************** */
    void sample_speed();
    void configure_speed_sampling();

    /* **************************************** */
    // Dependencies and internal state
    /* **************************************** */
    friend class PhaseInterface;
    friend struct sim::LevelSim;
    friend struct sim::PlayerSimTestAccess;

    PlayerSimConfig config{};
    EntityLedger& ledger_;
    CombatEvents const& combat_events_;
    HealthTable& health_table_;
    SpatialQueryManager const& spatial_query_manager;
    lasers::Sim& lasers;
    SimClock const& simulation_clock;
    HealthIndex health_index_{};
    Health max_health_{1000};
    bool death_notification_pending{false};
};
} // namespace ioj::sim::player
