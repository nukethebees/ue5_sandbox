#pragma once
#include <ioj/sim/entity_types.h>
#include <ioj/sim/laser_source.h>
#include <ioj/sim/player/command_interface.h>
#include <ioj/sim/player/fire_rate.h>
#include <ioj/sim/player/laser_firing_state.h>
#include <ioj/sim/player/ship_laser_mode.h>
#include <ioj/sim/player/sim.h>
#include <ioj/sim/player/space_ship_common.h>
#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/ships/common/ShipLaserModeConversion.h>
#include <SpaceGame/ships/player/TestShipFireRateConversion.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGamePresentation/entities/ShipHealth.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGamePresentation/presentation/PlayerPresentation.h>
#include <SpaceGamePresentation/support/logging/ActorLoggingConfig.h>

#include <CoreMinimal.h>
#include <GameFramework/Pawn.h>

#include "TestSpaceShip.generated.h"

class ATestBatchOrchestrator;
class UCameraComponent;
class USpaceDustComponent;
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

    /* **************************************** */
    // Lifecycle and simulation binding
    /* **************************************** */
    ATestSpaceShip();
    auto make_spawn_data() const -> ::ioj::sim::player::PlayerSpawnData;
    void bind_simulation(::ioj::sim::player::CommandInterface& new_commands,
                         ::ioj::sim::player::Sim const& new_simulation);
    void unbind_simulation();
    auto has_simulation() const noexcept -> bool { return bound_simulation != nullptr; }

    /* **************************************** */
    // Entity identity and configuration
    /* **************************************** */
    auto get_unique_id() const noexcept -> ::ioj::sim::EntityUniqueId override;
    auto get_test_name() const noexcept -> FName { return TEXT("PlayerShip"); }
    auto get_team() const noexcept -> ETestTeam;
    void set_team(ETestTeam new_team) noexcept;

    void set_actor_config(FPlayerShipConfig const* new_config) noexcept;
    auto get_kills() const -> int32;

    /* **************************************** */
    // Flight controls
    /* **************************************** */
    void set_forward_input(float input);
    void set_right_input(float input);
    void set_up_input(float input);
    void set_pitch_input(float input);
    void set_yaw_input(float input);
    void set_roll_input(float input);
    void set_accelerator(float input);
    void select_flight_model_slot(::ioj::sim::player::FlightModelSlot slot);
    auto get_active_flight_model_slot() const -> ::ioj::sim::player::FlightModelSlot;
    auto set_flight_model_slot_profile(::ioj::sim::player::FlightModelSlot slot,
                                       ::ioj::sim::player::FlightModelProfile profile) -> bool;
    auto get_active_flight_model_profile() const -> ::ioj::sim::player::FlightModelProfile;
    void start_boost();
    void stop_boost();
    void start_brake();
    void start_emergency_brake();
    void stop_emergency_brake();
    void stop_brake();
    auto get_velocity() const -> FVector;
    auto get_speed() const -> float;
    auto get_persistent_forward_target_speed() const -> float;
    auto get_sampled_target_speed_scale() const -> FVector2D;

    /* **************************************** */
    // Energy and weapons
    /* **************************************** */
    auto energy_is_full() const -> bool;
    auto get_energy() const -> float;

    auto get_lock_on_target() const -> ::ioj::sim::EntityUniqueId;
    void start_fire_laser();
    void stop_fire_laser();
    void upgrade_laser();
    auto get_laser_fire_rate() const noexcept -> ETestShipFireRate;
    auto get_laser_firing_mode() const noexcept -> ::ioj::sim::LaserFiringState;
    void select_next_laser_fire_rate() noexcept;
    void select_previous_laser_fire_rate() noexcept;
    void set_laser_fire_rate(ETestShipFireRate value) noexcept;

    /* **************************************** */
    // Health and collision
    /* **************************************** */
    void add_health(int32 added_health);
    auto get_health_info() const -> FShipHealth;
    auto is_alive() const noexcept -> bool;

    auto get_collision_mesh() const -> UStaticMesh const*;
    auto get_middle_socket() const -> FTransform;

    FOnPlayerShipDied on_player_ship_died;

#if WITH_EDITOR
    /* **************************************** */
    // Diagnostics
    /* **************************************** */
    auto get_speed_samples() const noexcept -> std::span<ml::Vector2d const>;
    auto get_speed_sample_index() const noexcept -> int32;
#endif

    /* **************************************** */
    // Presentation
    /* **************************************** */
    auto get_presentation_resources() const -> FPlayerPresentationResources;

#if !UE_BUILD_SHIPPING
    void apply_space_dust_debug_preset(FStringView preset);
#endif
  private:
    /* **************************************** */
    // Simulation access and presentation
    /* **************************************** */
    auto GetVelocity() const -> FVector override;

    auto commands() -> ::ioj::sim::player::CommandInterface&;
    auto simulation() const -> ::ioj::sim::player::Sim const&;

    void handle_simulation_death();
    void configure_ship_mesh();

    /* **************************************** */
    // State
    /* **************************************** */
    FPlayerShipConfig const* actor_config{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    ETestTeam team{ETestTeam::White};

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    UCameraComponent* camera{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Presentation", meta = (AllowPrivateAccess))
    USpaceDustComponent* space_dust{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    UStaticMeshComponent* ship_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Niagara", meta = (AllowPrivateAccess))
    UNiagaraComponent* boost_pulse{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Niagara", meta = (AllowPrivateAccess))
    UNiagaraComponent* boost_engine_effect{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Laser", meta = (AllowPrivateAccess))
    EShipLaserMode laser_mode{EShipLaserMode::Single};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Laser", meta = (AllowPrivateAccess))
    ETestShipFireRate laser_fire_rate{ETestShipFireRate::Burst3};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Health", meta = (AllowPrivateAccess))
    FShipHealth health{1000};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Logging", meta = (AllowPrivateAccess))
    FActorLoggingConfig log_config{1.f};

    ::ioj::sim::player::FlightModelLoadout flight_models_{
        ::ioj::sim::player::make_default_flight_model_loadout()};

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

    ::ioj::sim::player::CommandInterface* bound_commands_{nullptr};
    ::ioj::sim::player::Sim const* bound_simulation{nullptr};
};
