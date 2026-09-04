#pragma once

#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGame/entities/TestEntityUniqueId.h>
#include <SpaceGame/entities/TestTeam.h>
#include <SpaceGame/ships/common/LaserFiringState.h>
#include <SpaceGame/ships/common/ShipHealthComponent.h>
#include <SpaceGame/ships/common/ShipLaserMode.h>
#include <SpaceGame/ships/common/SpaceShipCommon.h>
#include <SpaceGame/ships/common/SpaceShipFlightModel.h>
#include <SpaceGame/ships/player/TestShipFireRate.h>
#include <SpaceGame/ships/player/TestSpaceShipControlMode.h>
#include <SpaceGame/ships/player/TestSpaceShipFlightMode.h>

#include <CoreMinimal.h>

namespace ml::test_space_ship {
struct SPACEGAME_API Simulation {
    TestEntityUniqueId unique_entity_id;
    FRegistryEntityHandle registry_handle{};
    ETestTeam team{ETestTeam::White};

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
};
} // namespace ml::test_space_ship
