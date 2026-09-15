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
}

namespace ioj::sim::lasers {
struct Sim;
}

namespace ioj::sim::player {
class PhaseInterface;

struct PlayerSpawnData {
    PlayerSimConfig config;
    ioj::sim::Team team{ioj::sim::Team::White};

    ioj::sim::Transform3d transform{ioj::sim::Transform3d{}};
    ioj::sim::Transform3d body_transform{ioj::sim::Transform3d{}};
    ioj::sim::Transform3d left_socket{ioj::sim::Transform3d{}};
    ioj::sim::Transform3d right_socket{ioj::sim::Transform3d{}};
    ioj::sim::Transform3d middle_socket{ioj::sim::Transform3d{}};

    ioj::sim::SpaceShipFlightMode flight_mode{ioj::sim::SpaceShipFlightMode::ForwardSpeed};
    ioj::sim::SpaceShipControlMode control_mode{ioj::sim::SpaceShipControlMode::Velocity};

    ioj::sim::ShipLaserMode laser_mode{ioj::sim::ShipLaserMode::Single};
    ioj::sim::ShipFireRate laser_fire_rate{ioj::sim::ShipFireRate::Burst3};

    ioj::sim::ShipHealth health{1000};
};

struct Sim {
    using RegistryEntityData = ioj::sim::RegistryEntityData;

    /* **************************************** */
    // Construction and configuration
    /* **************************************** */
    Sim(SimClock const& clock,
        EntityRegistry& entity_registry,
        SpatialQueryManager const& spatial_query_manager,
        ioj::sim::lasers::Sim& lasers);
    Sim(Sim const&) = delete;
    Sim(Sim&&) = delete;
    auto operator=(Sim const&) -> Sim& = delete;
    auto operator=(Sim&&) -> Sim& = delete;

    auto get_read_view() const -> PlayerReadView {
        return {transform,
                body_transform,
                get_middle_socket(),
                velocity,
                boost_brake_state,
                laser_firing_mode,
                boost_start_sequence_};
    }
    void configure(PlayerSpawnData const& spawn) noexcept;
    void set_config(PlayerSimConfig const& new_config) noexcept;
    void set_team(ioj::sim::Team new_team) noexcept { team = new_team; }
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
    void set_flight_mode(ioj::sim::SpaceShipFlightMode new_flight_mode) noexcept;

    /* **************************************** */
    // Weapons
    /* **************************************** */
    void start_fire_laser();
    void stop_fire_laser();
    void upgrade_laser() noexcept;
    void select_next_laser_fire_rate() noexcept;
    void select_previous_laser_fire_rate() noexcept;
    void set_laser_fire_rate(ioj::sim::ShipFireRate value) noexcept;

    /* **************************************** */
    // Health and status
    /* **************************************** */
    void add_health(std::int32_t added_health);
    auto consume_death_notification() noexcept -> bool;

    auto get_kills() const -> std::int32_t;
    auto get_speed() const noexcept -> float;
    auto get_energy() const -> float;
    auto energy_is_full() const -> bool;
    auto get_middle_socket() const -> ioj::sim::Transform3d;
    auto get_laser_effective_range() const noexcept -> float;

    /* **************************************** */
    // Sim state
    /* **************************************** */
    ioj::sim::EntityUniqueId unique_entity_id;
    RegistryEntityHandle registry_handle{};
    ioj::sim::Team team{ioj::sim::Team::White};

    ioj::sim::Transform3d transform{ioj::sim::Transform3d{}};
    ioj::sim::Transform3d body_transform{ioj::sim::Transform3d{}};
    ioj::sim::Transform3d left_socket{ioj::sim::Transform3d{}};
    ioj::sim::Transform3d right_socket{ioj::sim::Transform3d{}};
    ioj::sim::Transform3d middle_socket{ioj::sim::Transform3d{}};

    float thrust_energy{1.f};
    float thrust_change_rate{0.f};

    ioj::sim::ShipFlightModel<float> forward_flight_model{};
    ioj::sim::ShipFlightModel<ml::Vector3d> planar_flight_model{};
    ioj::sim::ShipFlightModel<float> planar_boost_flight_model{};
    ioj::sim::SpaceShipFlightMode flight_mode{ioj::sim::SpaceShipFlightMode::ForwardSpeed};
    ioj::sim::SpaceShipControlMode control_mode{ioj::sim::SpaceShipControlMode::Velocity};
    ml::Vector3d velocity{ml::Vector3d{}};
    ml::Vector3d planar_velocity{ml::Vector3d{}};
    float planar_boost_speed{0.f};
    float target_speed{0.f};
    ml::Vector2d target_local_planar_velocity_scale{ml::Vector2d{}};
    ml::Vector3d target_local_planar_velocity{ml::Vector3d{}};
    ml::Vector2d planar_movement_direction{ml::Vector2d{}};
    ioj::sim::player::BoostBrakeState boost_brake_state{ioj::sim::player::BoostBrakeState::None};
    ml::Vector2d rotation_input{ml::Vector2d{}};
    float roll_input{0.f};
    float time_since_rotation_input{100.f};

    ioj::sim::ShipLaserMode laser_mode{ioj::sim::ShipLaserMode::Single};
    float laser_shot_cooldown{0.f};
    std::int32_t lasers_fired_this_burst{0};
    std::int32_t lasers_per_burst{3};
    RegistryEntityHandle lock_on_target{};
    ioj::sim::LaserFiringState laser_firing_mode{ioj::sim::LaserFiringState::idle};
    ioj::sim::ShipFireRate laser_fire_rate{ioj::sim::ShipFireRate::Burst3};

    ioj::sim::ShipHealth health{1000};
    bool sampling{false};
    bool speed_sampling_enabled{};

    std::int32_t speed_sample_index{0};
    std::int32_t speed_sample_max{0};
    std::int32_t speed_sample_ticks_remaining{0};
    std::int32_t speed_sample_tick_period{1};
    std::vector<ml::Vector2d> speed_samples;
  private:
    /* **************************************** */
    // Tick phases
    /* **************************************** */
    void begin_play();
    void update_timers(float dt);
    void move(float dt);
    void queue_commands();
    void resolve_damage_events();
    void update_entity_registry();

    /* **************************************** */
    // Registry integration
    /* **************************************** */
    void register_with_entity_registry();
    auto get_entity_update_data() const -> RegistryEntityData;
    void queue_entity_update(EntityDeathInfo const& death_info);

    /* **************************************** */
    // Movement
    /* **************************************** */
    void integrate_velocity(float dt);
    void update_rotation(float dt);
    void update_body_orientation(float dt);
    void set_desired_planar_velocity(ml::Vector3d desired_velocity);
    std::uint64_t boost_start_sequence_{};
    void set_boost_brake_state(ioj::sim::player::BoostBrakeState state);
    void update_boost_brake(float dt);

    /* **************************************** */
    // Weapons
    /* **************************************** */
    void set_lock_on_target(RegistryEntityHandle target) noexcept;
    void set_laser_mode(ioj::sim::LaserFiringState mode) noexcept;
    void update_laser_firing();
    void fire_laser();
    void fire_lasers_from(std::span<ioj::sim::Transform3d const> fire_points);

    /* **************************************** */
    // Health
    /* **************************************** */
    void set_health(std::int32_t new_health, RegistryEntityHandle killer = {});
    void die(RegistryEntityHandle killer);

    /* **************************************** */
    // Diagnostics
    /* **************************************** */
    void sample_speed();
    void configure_speed_sampling();

    /* **************************************** */
    // Dependencies and internal state
    /* **************************************** */
    friend class PhaseInterface;

    PlayerSimConfig config{};
    EntityRegistry& entity_registry;
    SpatialQueryManager const& spatial_query_manager;
    ioj::sim::lasers::Sim& lasers;
    SimClock const& simulation_clock;
    bool death_notification_pending{false};
};
} // namespace ioj::sim::player
