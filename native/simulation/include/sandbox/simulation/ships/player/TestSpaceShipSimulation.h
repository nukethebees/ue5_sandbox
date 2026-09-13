#pragma once
#include <cassert>
#include <cmath>
#include <cstdint>
#include <optional>
#include <sandbox/core/diagnostics.h>
#include <sandbox/core/vector2d.h>
#include <sandbox/simulation/ship_health.h>
#include <sandbox/simulation/ships/player/PlayerReadView.h>
#include <sandbox/simulation/transform3d.h>
#include <span>
#include <vector>

#include <sandbox/simulation/simulation/LevelSimulationConfig.h>

#include <sandbox/simulation/entity_handle.h>
#include <sandbox/simulation/entity_types.h>
#include <sandbox/simulation/registry_entity_data.h>
#include <sandbox/simulation/ship_flight_model.h>
#include <sandbox/simulation/ships/common/LaserFiringState.h>
#include <sandbox/simulation/ships/common/ShipLaserMode.h>
#include <sandbox/simulation/ships/common/SpaceShipCommon.h>
#include <sandbox/simulation/ships/player/TestShipFireRate.h>
#include <sandbox/simulation/ships/player/TestSpaceShipControlMode.h>
#include <sandbox/simulation/ships/player/TestSpaceShipFlightMode.h>
#include <sandbox/simulation/simulation/SimulationClock.h>

struct FLevelSimulation;
struct EntityDeathInfo;
struct FPlayerSimulationConfig;
struct FTestEntityRegistry;

namespace ml {
struct FSpatialQueryManager;
}

namespace ml::test_lasers {
struct Simulation;
}

namespace ml::test_space_ship {
class PhaseInterface;

struct FPlayerSpawnData {
    FPlayerSimulationConfig config;
    ml::simulation::Team team{ml::simulation::Team::White};

    ml::simulation::Transform3d transform{ml::simulation::Transform3d{}};
    ml::simulation::Transform3d body_transform{ml::simulation::Transform3d{}};
    ml::simulation::Transform3d left_socket{ml::simulation::Transform3d{}};
    ml::simulation::Transform3d right_socket{ml::simulation::Transform3d{}};
    ml::simulation::Transform3d middle_socket{ml::simulation::Transform3d{}};

    ml::simulation::SpaceShipFlightMode flight_mode{
        ml::simulation::SpaceShipFlightMode::ForwardSpeed};
    ml::simulation::SpaceShipControlMode control_mode{
        ml::simulation::SpaceShipControlMode::Velocity};

    ml::simulation::ShipLaserMode laser_mode{ml::simulation::ShipLaserMode::Single};
    ml::simulation::ShipFireRate laser_fire_rate{ml::simulation::ShipFireRate::Burst3};

    ml::simulation::ShipHealth health{1000};
};

struct Simulation {
    using RegistryEntityData = ml::simulation::RegistryEntityData;

    /* **************************************** */
    // Construction and configuration
    /* **************************************** */
    Simulation(FSimulationClock const& clock,
               FTestEntityRegistry& entity_registry,
               FSpatialQueryManager const& spatial_query_manager,
               ml::test_lasers::Simulation& lasers);
    Simulation(Simulation const&) = delete;
    Simulation(Simulation&&) = delete;
    auto operator=(Simulation const&) -> Simulation& = delete;
    auto operator=(Simulation&&) -> Simulation& = delete;

    auto get_read_view() const -> FPlayerReadView {
        return {transform,
                body_transform,
                get_middle_socket(),
                velocity,
                boost_brake_state,
                laser_firing_mode,
                boost_start_sequence_};
    }
    void set_config(FPlayerSimulationConfig const& new_config) noexcept;

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
    void set_flight_mode(ml::simulation::SpaceShipFlightMode new_flight_mode) noexcept;

    /* **************************************** */
    // Weapons
    /* **************************************** */
    void start_fire_laser();
    void stop_fire_laser();
    void upgrade_laser() noexcept;
    void select_next_laser_fire_rate() noexcept;
    void select_previous_laser_fire_rate() noexcept;
    void set_laser_fire_rate(ml::simulation::ShipFireRate value) noexcept;

    /* **************************************** */
    // Health and status
    /* **************************************** */
    void add_health(std::int32_t added_health);
    auto consume_death_notification() noexcept -> bool;

    auto get_kills() const -> std::int32_t;
    auto get_speed() const noexcept -> float;
    auto get_energy() const -> float;
    auto energy_is_full() const -> bool;
    auto get_middle_socket() const -> ml::simulation::Transform3d;
    auto get_laser_effective_range() const noexcept -> float;

    /* **************************************** */
    // Simulation state
    /* **************************************** */
    ml::simulation::EntityUniqueId unique_entity_id;
    FRegistryEntityHandle registry_handle{};
    ml::simulation::Team team{ml::simulation::Team::White};

    ml::simulation::Transform3d transform{ml::simulation::Transform3d{}};
    ml::simulation::Transform3d body_transform{ml::simulation::Transform3d{}};
    ml::simulation::Transform3d left_socket{ml::simulation::Transform3d{}};
    ml::simulation::Transform3d right_socket{ml::simulation::Transform3d{}};
    ml::simulation::Transform3d middle_socket{ml::simulation::Transform3d{}};

    float thrust_energy{1.f};
    float thrust_change_rate{0.f};

    ml::simulation::ShipFlightModel<float> forward_flight_model{};
    ml::simulation::ShipFlightModel<ml::Vector3d> planar_flight_model{};
    ml::simulation::ShipFlightModel<float> planar_boost_flight_model{};
    ml::simulation::SpaceShipFlightMode flight_mode{
        ml::simulation::SpaceShipFlightMode::ForwardSpeed};
    ml::simulation::SpaceShipControlMode control_mode{
        ml::simulation::SpaceShipControlMode::Velocity};
    ml::Vector3d velocity{ml::Vector3d{}};
    ml::Vector3d planar_velocity{ml::Vector3d{}};
    float planar_boost_speed{0.f};
    float target_speed{0.f};
    ml::Vector2d target_local_planar_velocity_scale{ml::Vector2d{}};
    ml::Vector3d target_local_planar_velocity{ml::Vector3d{}};
    ml::Vector2d planar_movement_direction{ml::Vector2d{}};
    ml::simulation::player::BoostBrakeState boost_brake_state{
        ml::simulation::player::BoostBrakeState::None};
    ml::Vector2d rotation_input{ml::Vector2d{}};
    float roll_input{0.f};
    float time_since_rotation_input{100.f};

    ml::simulation::ShipLaserMode laser_mode{ml::simulation::ShipLaserMode::Single};
    float laser_shot_cooldown{0.f};
    std::int32_t lasers_fired_this_burst{0};
    std::int32_t lasers_per_burst{3};
    FRegistryEntityHandle lock_on_target{};
    ml::simulation::LaserFiringState laser_firing_mode{ml::simulation::LaserFiringState::idle};
    ml::simulation::ShipFireRate laser_fire_rate{ml::simulation::ShipFireRate::Burst3};

    ml::simulation::ShipHealth health{1000};
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
    void set_boost_brake_state(ml::simulation::player::BoostBrakeState state);
    void update_boost_brake(float dt);

    /* **************************************** */
    // Weapons
    /* **************************************** */
    void set_lock_on_target(FRegistryEntityHandle target) noexcept;
    void set_laser_mode(ml::simulation::LaserFiringState mode) noexcept;
    void update_laser_firing();
    void fire_laser();
    void fire_lasers_from(std::span<ml::simulation::Transform3d const> fire_points);

    /* **************************************** */
    // Health
    /* **************************************** */
    void set_health(std::int32_t new_health, FRegistryEntityHandle killer = {});
    void die(FRegistryEntityHandle killer);

    /* **************************************** */
    // Diagnostics
    /* **************************************** */
    void sample_speed();
    void configure_speed_sampling();

    /* **************************************** */
    // Dependencies and internal state
    /* **************************************** */
    friend class PhaseInterface;

    FPlayerSimulationConfig config{};
    FTestEntityRegistry& entity_registry;
    FSpatialQueryManager const& spatial_query_manager;
    ml::test_lasers::Simulation& lasers;
    FSimulationClock const& simulation_clock;
    bool death_notification_pending{false};
};
} // namespace ml::test_space_ship
