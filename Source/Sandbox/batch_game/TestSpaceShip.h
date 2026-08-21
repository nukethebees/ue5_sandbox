#pragma once

#include <Sandbox/batch_game/SimulationClockInterface.h>
#include <Sandbox/batch_game/SpatialQueryHit.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h>
#include <Sandbox/batch_game/TestEntity.h>
#include <Sandbox/batch_game/TestShipFireRate.h>
#include <Sandbox/batch_game/TestSpaceShipControlMode.h>
#include <Sandbox/batch_game/TestSpaceShipFlightMode.h>
#include <Sandbox/health/ShipHealthComponent.h>
#include <Sandbox/logging/ActorLoggingConfig.h>
#include <Sandbox/players/LaserFiringState.h>
#include <Sandbox/players/ShipLaserMode.h>
#include <Sandbox/players/SpaceShipCommon.h>
#include <Sandbox/players/SpaceShipFlightModel.h>
#include <SandboxNative/RegistryEntityHandle.h>

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "TestSpaceShip.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UNiagaraSystem;
class UNiagaraComponent;

class AShipLaser;
class UShipLaserConfig;
class AShipHomingLaser;
class AShipBomb;
class UShipHealthComponent;
struct FTestEntityRegistry;
class ATestLasers;
class UTestSpaceShipData;
struct EntityDeathInfo;
struct FTestSpaceShipSpatialQueryAccess;

namespace ml::test_space_ship {
class PhaseInterface;
}

namespace ml::entity_registry {
struct EntityData;
}

DECLARE_DELEGATE(FOnPlayerShipDied);

UCLASS()
class SANDBOX_API ATestSpaceShip
    : public APawn
    , public ITestEntity {
    GENERATED_BODY()
    friend class ml::test_space_ship::PhaseInterface;
  public:
    using RegistryEntityData = ml::entity_registry::EntityData;

    static constexpr bool sweep_movement{false};

    struct Sockets {
        inline static FName const left{"Left"};
        inline static FName const right{"Right"};
        inline static FName const middle{"Middle"};
    };

    ATestSpaceShip();

    // ITestEntity
    auto get_entity_handle() const noexcept -> FRegistryEntityHandle override {
        return registry_handle;
    }
    auto get_test_name() const noexcept -> FName { return TEXT("PlayerShip"); }
    /* ------------------------------------------------------------------------------------------ */
    // Entity data
    /* ------------------------------------------------------------------------------------------ */
    auto get_unique_id() const -> TestEntityUniqueId;
    auto get_entity_registry_handle() const -> FRegistryEntityHandle;
    auto get_team() const noexcept -> ETestTeam;

    auto get_entity_registry() const { return entity_registry; }
    void set_entity_registry(FTestEntityRegistry* er) { entity_registry = er; }

    void bind_simulation_clock(ATestBatchOrchestrator const& orchestrator);

    auto get_laser_actor() const { return laser_actor; }
    void set_laser_actor(ATestLasers* actor) { laser_actor = actor; }

    auto get_actor_config() const { return actor_config; }
    void set_actor_config(UTestSpaceShipData* const new_config) noexcept {
        actor_config = new_config;
    }

    auto get_kills() const -> int32;

    /* ------------------------------------------------------------------------------------------ */
    // Movement
    /* ------------------------------------------------------------------------------------------ */
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
    auto get_move_input() const { return planar_movement_direction; }
    auto get_control_mode() const { return control_mode; }
    auto get_flight_mode() const { return flight_mode; }
    auto get_target_local_planar_velocity_scale() const {
        return target_local_planar_velocity_scale;
    }
    auto get_target_local_planar_velocity() const { return target_local_planar_velocity; }
    auto get_turn_input() const { return rotation_input; }

    // Energy
    bool energy_is_full() const;
    auto get_energy() const -> float;

    /* ------------------------------------------------------------------------------------------ */
    // Combat
    /* ------------------------------------------------------------------------------------------ */
    auto get_lock_on_target() const -> AActor const* { return lock_on_target; }

    // Combat - laser
    void start_fire_laser();
    void stop_fire_laser();
    void upgrade_laser();

    auto get_laser_fire_rate() const noexcept -> ETestShipFireRate { return laser_fire_rate; }
    auto get_laser_firing_mode() const noexcept -> ELaserFiringState { return laser_firing_mode; }
    void select_next_laser_fire_rate() noexcept;
    void select_previous_laser_fire_rate() noexcept;
    void set_laser_fire_rate(ETestShipFireRate const value) noexcept;

    // Combat - bomb
    void fire_bomb();
    void add_bomb();
    auto get_bombs() const { return bombs; }

    /* ------------------------------------------------------------------------------------------ */
    // Health
    /* ------------------------------------------------------------------------------------------ */
    void add_health(int32 added_health);
    auto get_health_info() const -> FShipHealth { return health; }
    bool is_alive() const noexcept { return health.is_alive(); }

    static constexpr auto tick_clamp(auto value, auto delta_time, auto abs_max_value) {
        return FMath::Clamp(value * delta_time, -abs_max_value, abs_max_value);
    }
    static constexpr auto clamp(auto value, auto abs_max_value) {
        return FMath::Clamp(value, -abs_max_value, abs_max_value);
    }

    // Mesh
    auto get_ship_forward_vector() const -> FVector;
    auto get_middle_socket() const -> FTransform;

    // Delegates
    FOnShipSpeedChanged on_speed_changed;
    FOnShipTargetSpeedChanged on_target_speed_changed;
    FOnShipHealthChanged on_health_changed;
    FOnShipEnergyChanged on_energy_changed;
    FOnShipBombsChanged on_bombs_changed;
    FOnLaserModeChanged on_laser_mode_changed;
    FOnLockOnAcquired on_lock_on_acquired;
    FOnShipFireRateChanged on_ship_fire_rate_changed;
    FOnPlayerShipDied on_player_ship_died;

#if WITH_EDITORONLY_DATA
    FOnSpeedSampled on_speed_sampled;
#endif

#if WITH_EDITOR
    auto get_speed_samples() const noexcept -> TConstArrayView<FVector2d> { return speed_samples; }
    auto get_speed_sample_index() const noexcept -> int32 { return speed_sample_index; }
#endif
  private:
    /* ------------------------------------------------------------------------------------------ */
    // Life cycle
    /* ------------------------------------------------------------------------------------------ */
    void begin_play();
    void begin_tick();
    void update_timers(float const dt);
    void move(float const dt);
    void queue_commands();
    void resolve_damage_events();
    void update_entity_registry();
    void resolve_damage_targets();
    void sync_from_registry();
    void update_visual_data();
    void commit_visual_data();
    void end_tick();

    /* ------------------------------------------------------------------------------------------ */
    // Entity data
    /* ------------------------------------------------------------------------------------------ */
    void register_with_entity_registry();
    auto get_entity_update_data() const -> RegistryEntityData;

    /* ------------------------------------------------------------------------------------------ */
    // Movement
    /* ------------------------------------------------------------------------------------------ */
    auto GetVelocity() const -> FVector override;
    void set(EBoostBrakeState s);
    void update_boost_brake(float dt);
    void integrate_velocity(float dt);

    /* ------------------------------------------------------------------------------------------ */
    // Combat
    /* ------------------------------------------------------------------------------------------ */
    void set_lock_on_target(AActor* target);

    // Combat - laser
    void set_laser_mode(ELaserFiringState laser_mode);
    void update_laser_firing();
    void fire_laser();
    void fire_lasers_from(TConstArrayView<FTransform> const fire_points);

    // Combat - bomb
    void subtract_bomb();

    // Combat - homing laser
    void fire_homing_laser();

    // Visuals
    void configure_boost_pulse();
    void configure_boost_engine_effect();
    void configure_ship_mesh();

    void update_actor_rotation(float dt);
    void update_visual_orientation(float dt);

    /* ------------------------------------------------------------------------------------------ */
    // Health
    /* ------------------------------------------------------------------------------------------ */
    void set_health(int32 new_health);
    void die(FRegistryEntityHandle killer);
    void queue_entity_update(EntityDeathInfo const& death_info);

    // Mesh
    auto get_middle_socket(UStaticMeshComponent const& m) const -> FTransform;

    // Debugging
    void draw_debug_shapes();
#if WITH_EDITOR
    void sample_speed();
#endif
    void configure_speed_sampling();

    auto get_spatial_query_component() const -> UPrimitiveComponent const*;
    void resolve_hits(TConstArrayView<ml::FSpatialQueryHit> hits,
                      TArrayView<FRegistryEntityHandle> out_entity_handles) const;

    /* ------------------------------------------------------------------------------------------ */
    // Config
    /* ------------------------------------------------------------------------------------------ */
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    TObjectPtr<UTestSpaceShipData> actor_config{nullptr};

    /* ------------------------------------------------------------------------------------------ */
    // Entity data
    /* ------------------------------------------------------------------------------------------ */
    TestEntityUniqueId unique_entity_id;

    FTestEntityRegistry* entity_registry{nullptr};
    FRegistryEntityHandle registry_handle{};

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    ETestTeam team{ETestTeam::White};

    /* ------------------------------------------------------------------------------------------ */
    // Visuals
    /* ------------------------------------------------------------------------------------------ */
    // Camera
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    UCameraComponent* camera{nullptr};

    // Ship
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    UStaticMeshComponent* ship_mesh{nullptr};

    // Visuals - engine
    UPROPERTY(EditAnywhere, Category = "Sandbox|Niagara", meta = (AllowPrivateAccess))
    UNiagaraComponent* boost_pulse{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Niagara", meta = (AllowPrivateAccess))
    UNiagaraComponent* boost_engine_effect{nullptr};

    /* ------------------------------------------------------------------------------------------ */
    // Energy
    /* ------------------------------------------------------------------------------------------ */
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Energy", meta = (AllowPrivateAccess))
    float thrust_energy{1.f};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Energy", meta = (AllowPrivateAccess))
    float thrust_change_rate{0.f};

    /* ------------------------------------------------------------------------------------------ */
    // Movement
    /* ------------------------------------------------------------------------------------------ */
    // Movement - Speed
    TSpaceShipFlightModel<float> forward_flight_model{};
    TSpaceShipFlightModel<FVector> planar_flight_model{};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Speed", meta = (AllowPrivateAccess))
    ETestSpaceShipFlightMode flight_mode{ETestSpaceShipFlightMode::ForwardSpeed};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Movement", meta = (AllowPrivateAccess))
    ETestSpaceShipControlMode control_mode{ETestSpaceShipControlMode::Velocity};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Speed", meta = (AllowPrivateAccess))
    FVector velocity;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Speed", meta = (AllowPrivateAccess))
    FVector planar_velocity{FVector::ZeroVector};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Speed", meta = (AllowPrivateAccess))
    float target_speed{0.f};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Movement", meta = (AllowPrivateAccess))
    FVector2D target_local_planar_velocity_scale{FVector2D::ZeroVector};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Movement", meta = (AllowPrivateAccess))
    FVector target_local_planar_velocity{FVector::ZeroVector};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Movement", meta = (AllowPrivateAccess))
    FVector2D planar_movement_direction{FVector2D::ZeroVector};

    // Movement - Cruising
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Speed", meta = (AllowPrivateAccess))
    EBoostBrakeState boost_brake_state{EBoostBrakeState::None};

    // Movement - rotation
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Steering", meta = (AllowPrivateAccess))
    FVector2D rotation_input{FVector2D::ZeroVector};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Steering", meta = (AllowPrivateAccess))
    float roll_input{0.f};

    UPROPERTY(meta = (AllowPrivateAccess))
    float time_since_rotation_input{100.f};

    /* ------------------------------------------------------------------------ */
    /* Combat */
    /* ------------------------------------------------------------------------ */
    // Combat - Laser
    UPROPERTY(EditAnywhere, Category = "Sandbox|Laser", meta = (AllowPrivateAccess))
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Laser", meta = (AllowPrivateAccess))
    EShipLaserMode laser_mode{EShipLaserMode::Single};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Laser", meta = (AllowPrivateAccess))
    float laser_shot_cooldown{0.f};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Laser", meta = (AllowPrivateAccess))
    int32 lasers_fired_this_burst{0};
    int32 lasers_per_burst{3};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Laser", meta = (AllowPrivateAccess))
    AActor* lock_on_target{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Laser", meta = (AllowPrivateAccess))
    ELaserFiringState laser_firing_mode{ELaserFiringState::idle};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Laser", meta = (AllowPrivateAccess))
    ETestShipFireRate laser_fire_rate{ETestShipFireRate::Burst3};

    // Combat - Bombs
    UPROPERTY(EditAnywhere, Category = "Sandbox|Bomb", meta = (AllowPrivateAccess))
    int32 bombs{3};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Bomb", meta = (AllowPrivateAccess))
    TWeakObjectPtr<AShipBomb> active_bomb{nullptr};

    /* ------------------------------------------------------------------------------------------ */
    // Health
    /* ------------------------------------------------------------------------------------------ */
    UPROPERTY(EditAnywhere, Category = "Sandbox|Health", meta = (AllowPrivateAccess))
    FShipHealth health{1000};

    /* ------------------------------------------------------------------------------------------ */
    // Misc
    /* ------------------------------------------------------------------------------------------ */
    bool sampling{false};

    // Logging
    UPROPERTY(EditAnywhere, Category = "Sandbox|Logging", meta = (AllowPrivateAccess))
    FActorLoggingConfig log_config{1.f};

#if WITH_EDITORONLY_DATA
    int32 speed_sample_index{0};
    int32 speed_sample_max{0};
    int32 speed_sample_ticks_remaining{0};
    int32 speed_sample_tick_period{1};
    TArray<FVector2d> speed_samples;
    UPROPERTY(EditAnywhere, Category = "Debug", meta = (AllowPrivateAccess))
    bool debug_forward_socket_direction{false};
    UPROPERTY(EditAnywhere, Category = "Debug", meta = (AllowPrivateAccess))
    bool debug_forward_direction{false};
    UPROPERTY(EditAnywhere, Category = "Debug", meta = (AllowPrivateAccess))
    bool debug_lock_on{false};
    UPROPERTY(EditAnywhere, Category = "Debug", meta = (AllowPrivateAccess))
    float debug_lock_on_sphere_radius{1000.f};
#endif

    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;

    friend struct FTestSpaceShipSpatialQueryAccess;
};

struct FTestSpaceShipSpatialQueryAccess {
    ATestSpaceShip const* actor{nullptr};

    auto get_spatial_query_component() const -> UPrimitiveComponent const* {
        return actor->get_spatial_query_component();
    }

    void resolve_hits(TConstArrayView<ml::FSpatialQueryHit> const hits,
                      TArrayView<FRegistryEntityHandle> const out_entity_handles) const {
        actor->resolve_hits(hits, out_entity_handles);
    }
};
