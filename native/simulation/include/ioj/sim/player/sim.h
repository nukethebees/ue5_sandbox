#pragma once
#include <cassert>
#include <cmath>
#include <cstdint>
#include <ioj/sim/player/player_read_view.h>
#include <ioj/sim/ship_health.h>
#include <ioj/sim/transform3d.h>
#include <optional>
#include <sandbox/core/diagnostics.h>
#include <sandbox/core/vector2d.h>
#include <span>
#include <vector>

#include <ioj/sim/sim_config.h>

#include <ioj/sim/entity_handle.h>
#include <ioj/sim/entity_types.h>
#include <ioj/sim/player/control_mode.h>
#include <ioj/sim/player/fire_rate.h>
#include <ioj/sim/player/flight_mode.h>
#include <ioj/sim/player/laser_firing_state.h>
#include <ioj/sim/player/ship_laser_mode.h>
#include <ioj/sim/player/space_ship_common.h>
#include <ioj/sim/registry_entity_data.h>
#include <ioj/sim/ship_flight_model.h>
#include <ioj/sim/sim_clock.h>

namespace ioj::sim {
struct LevelSim;
struct EntityDeathInfo;
struct PlayerSimConfig;
struct EntityRegistry;
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

    SpaceShipFlightMode flight_mode{SpaceShipFlightMode::ForwardSpeed};
    SpaceShipControlMode control_mode{SpaceShipControlMode::Velocity};

    ShipLaserMode laser_mode{ShipLaserMode::Single};
    ShipFireRate laser_fire_rate{ShipFireRate::Burst3};

    ShipHealth health{1000};
};

struct MovementState {
    Transform3d transform{};
    Transform3d body_transform{};
    ml::Vector3d velocity{};
    ml::Vector3d planar_velocity{};
    float planar_boost_speed{};
    float thrust_energy{1.f};
    float thrust_change_rate{};
    float target_speed{};
    ShipFlightModel<float> forward_flight_model{};
    ShipFlightModel<ml::Vector3d> planar_flight_model{};
    ShipFlightModel<float> planar_boost_flight_model{};
    BoostBrakeState boost_brake_state{};
    float time_since_rotation_input{100.f};
    std::uint64_t boost_start_sequence{};
};

struct Sim {
    using RegistryEntityData = sim::RegistryEntityData;

    /* **************************************** */
    // Construction and configuration
    /* **************************************** */
    Sim(SimClock const& clock,
        EntityRegistry& entity_registry,
        SpatialQueryManager const& spatial_query_manager,
        lasers::Sim& lasers);
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    auto get_read_view() const -> PlayerReadView {
        return {movement_state_.transform,
                movement_state_.body_transform,
                get_middle_socket(),
                movement_state_.velocity,
                movement_state_.boost_brake_state,
                laser_firing_mode,
                movement_state_.boost_start_sequence};
    }
    auto get_movement_state() const noexcept -> MovementState const& { return movement_state_; }
    void configure(PlayerSpawnData const& spawn) noexcept;
    void set_config(PlayerSimConfig const& new_config) noexcept;
    void set_team(Team new_team) noexcept { team = new_team; }
    void set_speed_sampling_enabled(bool enabled) noexcept { speed_sampling_enabled = enabled; }

    /* **************************************** */
    // Flight controls
    /* **************************************** */
    void set_move_input(ml::Vector2d input) noexcept;
    void set_lateral_move_input(float input) noexcept;
    void set_vertical_move_input(float input) noexcept;
    void set_ship_2d_control(ml::Vector2d input);
    void set_ship_1d_control_x(float input);
    void set_ship_1d_control_y(float input);
    void select_next_control_mode();
    void select_previous_control_mode();
    void start_sampling() noexcept;
    void stop_sampling();
    void adjust_desired_forward_velocity(float direction);
    void turn(ml::Vector2d direction) noexcept;
    void start_boost();
    void stop_boost();
    void start_brake();
    void stop_brake();
    void roll(float direction) noexcept;
    void set_flight_mode(SpaceShipFlightMode new_flight_mode) noexcept;

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
    auto consume_death_notification() noexcept -> bool;

    auto get_kills() const -> std::int32_t;
    auto get_speed() const noexcept -> float;
    auto get_energy() const -> float;
    auto energy_is_full() const -> bool;
    auto get_middle_socket() const -> Transform3d;
    auto get_laser_effective_range() const noexcept -> float;

    /* **************************************** */
    // Sim state
    /* **************************************** */
    EntityUniqueId unique_entity_id;
    RegistryEntityHandle registry_handle{};
    Team team{Team::White};

    Transform3d left_socket{Transform3d{}};
    Transform3d right_socket{Transform3d{}};
    Transform3d middle_socket{Transform3d{}};

    SpaceShipFlightMode flight_mode{SpaceShipFlightMode::ForwardSpeed};
    SpaceShipControlMode control_mode{SpaceShipControlMode::Velocity};
    ml::Vector2d target_local_planar_velocity_scale{ml::Vector2d{}};
    ml::Vector3d target_local_planar_velocity{ml::Vector3d{}};
    ml::Vector2d planar_movement_direction{ml::Vector2d{}};
    ml::Vector2d rotation_input{ml::Vector2d{}};
    float roll_input{0.f};

    ShipLaserMode laser_mode{ShipLaserMode::Single};
    float laser_shot_cooldown{0.f};
    std::int32_t lasers_fired_this_burst{0};
    std::int32_t lasers_per_burst{3};
    EntityUniqueId lock_on_target{};
    LaserFiringState laser_firing_mode{LaserFiringState::idle};
    ShipFireRate laser_fire_rate{ShipFireRate::Burst3};

    ShipHealth health{1000};
    bool sampling{false};
    bool speed_sampling_enabled{};

    std::int32_t speed_sample_index{0};
    std::int32_t speed_sample_max{0};
    std::int32_t speed_sample_ticks_remaining{0};
    std::int32_t speed_sample_tick_period{1};
    std::vector<ml::Vector2d> speed_samples;
  private:
    MovementState movement_state_{};
    MovementState planned_movement_{};
    /* **************************************** */
    // Tick phases
    /* **************************************** */
    void begin_play();
    void prepare_tick(float dt);
    void think(float dt);
    void apply_movement();
    void generate_fire_commands();
    void resolve_damage_events();
    void update_entity_registry();

    /* **************************************** */
    // Registry integration
    /* **************************************** */
    void register_with_entity_registry();
    auto get_entity_update_data() const -> SingleAllocationRegistryEntityData;
    void queue_entity_update(EntityDeathInfo const& death_info);

    /* **************************************** */
    // Movement
    /* **************************************** */
    void integrate_velocity(float dt, MovementState& movement);
    void update_rotation(float dt, MovementState& movement);
    void update_body_orientation(float dt, MovementState& movement);
    void set_desired_planar_velocity(ml::Vector3d desired_velocity);
    std::uint64_t boost_start_sequence_{};
    void set_boost_brake_state(BoostBrakeState state);
    void set_boost_brake_state(BoostBrakeState state, MovementState& movement);
    void update_boost_brake(float dt, MovementState& movement);

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
    friend struct sim::PlayerSimTestAccess;

    PlayerSimConfig config{};
    EntityRegistry& entity_registry;
    SpatialQueryManager const& spatial_query_manager;
    lasers::Sim& lasers;
    SimClock const& simulation_clock;
    bool death_notification_pending{false};
};
} // namespace ioj::sim::player
