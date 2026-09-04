#pragma once

#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/ships/player/TestSpaceShipSimulation.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/support/logging/ActorLoggingConfig.h>

#include <CoreMinimal.h>
#include <GameFramework/Pawn.h>

#include "TestSpaceShip.generated.h"

class ATestBatchOrchestrator;
class UCameraComponent;
class UNiagaraComponent;
class UStaticMesh;
class UStaticMeshComponent;

DECLARE_DELEGATE(FOnPlayerShipDied);

UCLASS()
class SPACEGAME_API ATestSpaceShip
    : public APawn
    , public ITestEntity {
    GENERATED_BODY()
    friend class ATestBatchOrchestrator;
  public:
    struct Sockets {
        inline static FName const left{"Left"};
        inline static FName const right{"Right"};
        inline static FName const middle{"Middle"};
    };

    ATestSpaceShip();

    auto get_entity_handle() const noexcept -> FRegistryEntityHandle override;
    auto get_test_name() const noexcept -> FName { return TEXT("PlayerShip"); }
    auto get_unique_id() const -> TestEntityUniqueId;
    auto get_entity_registry_handle() const -> FRegistryEntityHandle;
    auto get_team() const noexcept -> ETestTeam;
    void set_team(ETestTeam new_team) noexcept;

    auto get_actor_config() const noexcept -> FPlayerShipConfig const* { return actor_config; }
    void set_actor_config(FPlayerShipConfig const* new_config) noexcept;
    auto get_kills() const -> int32;

    void set_move_input(FVector2D input);
    void set_lateral_move_input(float input);
    void set_vertical_move_input(float input);
    void set_ship_2d_control(FVector2D input);
    void set_ship_1d_control_x(float input);
    void set_ship_1d_control_y(float input);
    void select_next_control_mode();
    void select_previous_control_mode();
    void start_sampling();
    void stop_sampling();
    void turn(FVector2D direction);
    void start_boost();
    void stop_boost();
    void start_brake();
    void stop_brake();
    auto get_velocity() const -> FVector;
    auto get_speed() const -> float;
    void roll(float direction);
    auto get_target_speed() const -> float;
    auto get_move_input() const -> FVector2D;
    auto get_control_mode() const -> ETestSpaceShipControlMode;
    auto get_flight_mode() const -> ETestSpaceShipFlightMode;
    void set_flight_mode(ETestSpaceShipFlightMode new_flight_mode) noexcept;
    auto get_target_local_planar_velocity_scale() const -> FVector2D;
    auto get_target_local_planar_velocity() const -> FVector;
    auto get_turn_input() const -> FVector2D;

    auto energy_is_full() const -> bool;
    auto get_energy() const -> float;

    auto get_lock_on_target() const -> FRegistryEntityHandle;
    void start_fire_laser();
    void stop_fire_laser();
    void upgrade_laser();
    auto get_laser_fire_rate() const noexcept -> ETestShipFireRate;
    auto get_laser_firing_mode() const noexcept -> ELaserFiringState;
    void select_next_laser_fire_rate() noexcept;
    void select_previous_laser_fire_rate() noexcept;
    void set_laser_fire_rate(ETestShipFireRate value) noexcept;

    void add_health(int32 added_health);
    auto get_health_info() const -> FShipHealth;
    auto is_alive() const noexcept -> bool;

    auto get_collision_mesh() const -> UStaticMesh const*;
    auto get_ship_forward_vector() const -> FVector;
    auto get_middle_socket() const -> FTransform;

    FOnPlayerShipDied on_player_ship_died;

#if WITH_EDITOR
    auto get_speed_samples() const noexcept -> TConstArrayView<FVector2d>;
    auto get_speed_sample_index() const noexcept -> int32;
#endif
  private:
    auto GetVelocity() const -> FVector override;

    auto make_simulation() const -> ml::test_space_ship::Simulation;
    void bind_simulation(ml::test_space_ship::Simulation& new_simulation);
    void unbind_simulation();
    auto simulation() -> ml::test_space_ship::Simulation&;
    auto simulation() const -> ml::test_space_ship::Simulation const&;

    void begin_play_presentation();
    void update_visual_data(float dt);
    void commit_visual_data();
    void handle_simulation_death();
    void configure_boost_pulse();
    void configure_boost_engine_effect();
    void configure_ship_mesh();
    void draw_debug_shapes();

    FPlayerShipConfig const* actor_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    ETestTeam team{ETestTeam::White};

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    UCameraComponent* camera{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    UStaticMeshComponent* ship_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Niagara", meta = (AllowPrivateAccess))
    UNiagaraComponent* boost_pulse{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Niagara", meta = (AllowPrivateAccess))
    UNiagaraComponent* boost_engine_effect{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Speed", meta = (AllowPrivateAccess))
    ETestSpaceShipFlightMode flight_mode{ETestSpaceShipFlightMode::ForwardSpeed};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Movement", meta = (AllowPrivateAccess))
    ETestSpaceShipControlMode control_mode{ETestSpaceShipControlMode::Velocity};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Laser", meta = (AllowPrivateAccess))
    EShipLaserMode laser_mode{EShipLaserMode::Single};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Laser", meta = (AllowPrivateAccess))
    ETestShipFireRate laser_fire_rate{ETestShipFireRate::Burst3};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Health", meta = (AllowPrivateAccess))
    FShipHealth health{1000};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Logging", meta = (AllowPrivateAccess))
    FActorLoggingConfig log_config{1.f};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Debug", meta = (AllowPrivateAccess))
    bool debug_forward_socket_direction{false};
    UPROPERTY(EditAnywhere, Category = "Debug", meta = (AllowPrivateAccess))
    bool debug_forward_direction{false};
    UPROPERTY(EditAnywhere, Category = "Debug", meta = (AllowPrivateAccess))
    bool debug_lock_on{false};
    UPROPERTY(EditAnywhere, Category = "Debug", meta = (AllowPrivateAccess))
    float debug_lock_on_sphere_radius{1000.f};
#endif

    mutable TOptional<ml::test_space_ship::Simulation> standalone_simulation;
    ml::test_space_ship::Simulation* bound_simulation{nullptr};
    EBoostBrakeState presented_boost_brake_state{EBoostBrakeState::None};
};
