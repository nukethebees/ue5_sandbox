#pragma once
#include <SpaceGameSimulation/ships/player/PlayerReadView.h>

#include <SpaceGameSimulation/simulation/LevelSimulationConfig.h>

#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGameSimulation/entities/TestEntityRegistryData.h>
#include <SpaceGameSimulation/entities/TestEntityUniqueId.h>
#include <SpaceGameSimulation/entities/TestTeam.h>
#include <SpaceGameSimulation/ships/common/LaserFiringState.h>
#include <SpaceGameSimulation/ships/common/ShipHealth.h>
#include <SpaceGameSimulation/ships/common/ShipLaserMode.h>
#include <SpaceGameSimulation/ships/common/SpaceShipCommon.h>
#include <SpaceGameSimulation/ships/common/SpaceShipFlightModel.h>
#include <SpaceGameSimulation/ships/player/TestShipFireRate.h>
#include <SpaceGameSimulation/ships/player/TestSpaceShipControlMode.h>
#include <SpaceGameSimulation/ships/player/TestSpaceShipFlightMode.h>
#include <SpaceGameSimulation/simulation/SimulationClockInterface.h>

#include <CoreMinimal.h>

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
    ETestTeam team{ETestTeam::White};

    FTransform transform{FTransform::Identity};
    FTransform body_transform{FTransform::Identity};
    FTransform left_socket{FTransform::Identity};
    FTransform right_socket{FTransform::Identity};
    FTransform middle_socket{FTransform::Identity};
    float collision_radius{};

    ETestSpaceShipFlightMode flight_mode{ETestSpaceShipFlightMode::ForwardSpeed};
    ETestSpaceShipControlMode control_mode{ETestSpaceShipControlMode::Velocity};

    EShipLaserMode laser_mode{EShipLaserMode::Single};
    ETestShipFireRate laser_fire_rate{ETestShipFireRate::Burst3};

    FShipHealth health{1000};
};

struct SPACEGAMESIMULATION_API Simulation {
    using RegistryEntityData = ml::entity_registry::EntityData;

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
    void set_move_input(FVector2D input) noexcept;
    void set_lateral_move_input(float input) noexcept;
    void set_vertical_move_input(float input) noexcept;
    void set_ship_2d_control(FVector2D input);
    void set_ship_1d_control_x(float input);
    void set_ship_1d_control_y(float input);
    void select_next_control_mode();
    void select_previous_control_mode();
    void start_sampling() noexcept;
    void stop_sampling();
    void adjust_desired_forward_velocity(float direction);
    void turn(FVector2D direction) noexcept;
    void start_boost();
    void stop_boost();
    void start_brake();
    void stop_brake();
    void roll(float direction) noexcept;
    void set_flight_mode(ETestSpaceShipFlightMode new_flight_mode) noexcept;

    /* **************************************** */
    // Weapons
    /* **************************************** */
    void start_fire_laser();
    void stop_fire_laser();
    void upgrade_laser() noexcept;
    void select_next_laser_fire_rate() noexcept;
    void select_previous_laser_fire_rate() noexcept;
    void set_laser_fire_rate(ETestShipFireRate value) noexcept;

    /* **************************************** */
    // Health and status
    /* **************************************** */
    void add_health(int32 added_health);
    auto consume_death_notification() noexcept -> bool;

    auto get_kills() const -> int32;
    auto get_speed() const noexcept -> float;
    auto get_energy() const -> float;
    auto energy_is_full() const -> bool;
    auto get_middle_socket() const -> FTransform;
    auto get_laser_effective_range() const noexcept -> float;

    /* **************************************** */
    // Simulation state
    /* **************************************** */
    TestEntityUniqueId unique_entity_id;
    FRegistryEntityHandle registry_handle{};
    ETestTeam team{ETestTeam::White};

    FTransform transform{FTransform::Identity};
    FTransform body_transform{FTransform::Identity};
    FTransform left_socket{FTransform::Identity};
    FTransform right_socket{FTransform::Identity};
    FTransform middle_socket{FTransform::Identity};
    float collision_radius{0.f};

    float thrust_energy{1.f};
    float thrust_change_rate{0.f};

    TSpaceShipFlightModel<float> forward_flight_model{};
    TSpaceShipFlightModel<FVector> planar_flight_model{};
    ETestSpaceShipFlightMode flight_mode{ETestSpaceShipFlightMode::ForwardSpeed};
    ETestSpaceShipControlMode control_mode{ETestSpaceShipControlMode::Velocity};
    FVector velocity{FVector::ZeroVector};
    FVector planar_velocity{FVector::ZeroVector};
    float target_speed{0.f};
    FVector2D target_local_planar_velocity_scale{FVector2D::ZeroVector};
    FVector target_local_planar_velocity{FVector::ZeroVector};
    FVector2D planar_movement_direction{FVector2D::ZeroVector};
    EBoostBrakeState boost_brake_state{EBoostBrakeState::None};
    FVector2D rotation_input{FVector2D::ZeroVector};
    float roll_input{0.f};
    float time_since_rotation_input{100.f};

    EShipLaserMode laser_mode{EShipLaserMode::Single};
    float laser_shot_cooldown{0.f};
    int32 lasers_fired_this_burst{0};
    int32 lasers_per_burst{3};
    FRegistryEntityHandle lock_on_target{};
    ELaserFiringState laser_firing_mode{ELaserFiringState::idle};
    ETestShipFireRate laser_fire_rate{ETestShipFireRate::Burst3};

    FShipHealth health{1000};
    bool sampling{false};

#if WITH_EDITOR
    int32 speed_sample_index{0};
    int32 speed_sample_max{0};
    int32 speed_sample_ticks_remaining{0};
    int32 speed_sample_tick_period{1};
    TArray<FVector2d> speed_samples;
#endif
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
    void set_desired_planar_velocity(FVector desired_velocity);
    uint64 boost_start_sequence_{};
    void set_boost_brake_state(EBoostBrakeState state);
    void update_boost_brake(float dt);

    /* **************************************** */
    // Weapons
    /* **************************************** */
    void set_lock_on_target(FRegistryEntityHandle target) noexcept;
    void set_laser_mode(ELaserFiringState mode) noexcept;
    void update_laser_firing();
    void fire_laser();
    void fire_lasers_from(TConstArrayView<FTransform> fire_points);

    /* **************************************** */
    // Health
    /* **************************************** */
    void set_health(int32 new_health, FRegistryEntityHandle killer = {});
    void die(FRegistryEntityHandle killer);

    /* **************************************** */
    // Diagnostics
    /* **************************************** */
#if WITH_EDITOR
    void sample_speed();
#endif
    void configure_speed_sampling();

    /* **************************************** */
    // Dependencies and internal state
    /* **************************************** */
    friend class PhaseInterface;

    FPlayerSimulationConfig config{};
    FTestEntityRegistry& entity_registry;
    FSpatialQueryManager const& spatial_query_manager;
    ml::test_lasers::Simulation& lasers;
    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;
    bool death_notification_pending{false};
};
} // namespace ml::test_space_ship
