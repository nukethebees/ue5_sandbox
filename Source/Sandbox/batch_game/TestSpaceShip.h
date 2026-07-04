#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityOwnerId.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistryData.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h>
#include <Sandbox/batch_game/TestShipFireRate.h>
#include <Sandbox/health/ShipHealthComponent.h>
#include <Sandbox/logging/ActorLoggingConfig.h>
#include <Sandbox/players/BarrelRoll.h>
#include <Sandbox/players/LaserFiringMode.h>
#include <Sandbox/players/ShipLaserMode.h>
#include <Sandbox/players/SpaceShipCommon.h>
#include <Sandbox/players/SpaceShipFlightModel.h>
#include <Sandbox/players/SpeedResponse.h>

#include <SandboxCore/generation_index.h>

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Math/Color.h"

#include "TestSpaceShip.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UNiagaraSystem;
class UNiagaraComponent;

class AShipLaser;
class UShipLaserConfig;
class AShipHomingLaser;
class AShipBomb;
class UShipHealthComponent;
class ATestEntityRegistry;
class ATestLasers;
class UTestSpaceShipData;

UCLASS()
class ATestSpaceShip : public APawn {
    GENERATED_BODY()
  public:
    struct Sockets {
        inline static FName const left{"Left"};
        inline static FName const right{"Right"};
        inline static FName const middle{"Middle"};
    };

    ATestSpaceShip();

    // Life cycle
    void begin_play();
    void begin_tick();
    void tick(float const dt);
    void move(float const dt);
    void queue_commands();
    void resolve_hit_events();
    void update_entity_registry();
    void resolve_damage_targets();
    void sync_from_registry();
    void update_visuals();
    void end_tick();

    /* ------------------------------------------------------------------------------------------ */
    // Entity data
    /* ------------------------------------------------------------------------------------------ */
    void set_owner_id(TestEntityOwnerId const new_owner_id);
    auto get_owner_id() const -> TestEntityOwnerId;
    auto get_unique_id() const -> TestEntityUniqueId;
    auto get_entity_registry_handle() const -> FRegistryEntityHandle;
    auto get_team() const noexcept -> ETestTeam;

    /* ------------------------------------------------------------------------------------------ */
    // Movement
    /* ------------------------------------------------------------------------------------------ */
    void turn(FVector2D direction);
    void start_boost();
    void stop_boost();
    void start_brake();
    void stop_brake();
    auto get_velocity() const { return velocity; }
    auto get_speed() const { return get_velocity().Size(); }
    void roll(float direction);
    void barrel_roll(float direction);

    // Energy
    bool energy_is_full() const;

    /* ------------------------------------------------------------------------------------------ */
    // Combat
    /* ------------------------------------------------------------------------------------------ */
    auto get_lock_on_target() const -> AActor const* { return lock_on_target; }

    // Combat - laser
    void start_fire_laser();
    void stop_fire_laser();
    void upgrade_laser();

    auto get_laser_fire_rate() const noexcept -> ETestShipFireRate { return laser_fire_rate; }
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
    FOnShipHealthChanged on_health_changed;
    FOnShipEnergyChanged on_energy_changed;
    FOnShipBombsChanged on_bombs_changed;
    FOnLaserModeChanged on_laser_mode_changed;
    FOnLockOnAcquired on_lock_on_acquired;
    FOnShipFireRateChanged on_ship_fire_rate_changed;

#if WITH_EDITORONLY_DATA
    FOnSpeedSampled on_speed_sampled;
#endif
  protected:
    /* ------------------------------------------------------------------------------------------ */
    // Entity data
    /* ------------------------------------------------------------------------------------------ */
    void register_with_entity_registry();
    auto get_entity_update_data() const -> FTestEntityRegistryEntityData;

    /* ------------------------------------------------------------------------------------------ */
    // Movement
    /* ------------------------------------------------------------------------------------------ */
    void update_barrel_roll_timers(float const dt);
    auto GetVelocity() const -> FVector override;
    void set(EBoostBrakeState s);
    void update_boost_brake(this ATestSpaceShip& self, float const dt);
    void integrate_velocity(this ATestSpaceShip& self, float const dt);

    /* ------------------------------------------------------------------------------------------ */
    // Combat
    /* ------------------------------------------------------------------------------------------ */
    void set_lock_on_target(AActor* target);

    // Combat - laser
    void set_laser_mode(ELaserFiringMode laser_mode);
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

    void update_actor_rotation(this ATestSpaceShip& self, float const dt);
    void update_visual_orientation(this ATestSpaceShip& self, float const dt);

    /* ------------------------------------------------------------------------------------------ */
    // Health
    /* ------------------------------------------------------------------------------------------ */
    void set_health(int32 new_health);

    // Mesh
    auto get_middle_socket(UStaticMeshComponent const& m) const -> FTransform;

    // Debugging
    void draw_debug_shapes();
#if WITH_EDITOR
    void sample_speed();
#endif
    void configure_speed_sampling();

    /* ------------------------------------------------------------------------------------------ */
    // Config
    /* ------------------------------------------------------------------------------------------ */
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<UTestSpaceShipData> actor_config{nullptr};

    /* ------------------------------------------------------------------------------------------ */
    // Entity data
    /* ------------------------------------------------------------------------------------------ */
    TestEntityOwnerId owner_id{};
    TestEntityUniqueId unique_entity_id;

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};
    FRegistryEntityHandle registry_handle{};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    ETestTeam team{ETestTeam::White};

    /* ------------------------------------------------------------------------------------------ */
    // Visuals
    /* ------------------------------------------------------------------------------------------ */
    // Camera
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    UCameraComponent* camera{nullptr};

    // Ship
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    UStaticMeshComponent* ship_mesh{nullptr};

    // Visuals - engine
    UPROPERTY(EditAnywhere, Category = "Sandbox|Niagara")
    UNiagaraComponent* boost_pulse{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Niagara")
    UNiagaraComponent* boost_engine_effect{nullptr};

    /* ------------------------------------------------------------------------------------------ */
    // Energy
    /* ------------------------------------------------------------------------------------------ */
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Energy")
    float thrust_energy{1.f};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Energy")
    float thrust_change_rate{0.f};

    /* ------------------------------------------------------------------------------------------ */
    // Movement
    /* ------------------------------------------------------------------------------------------ */

    // Movement - Speed
    UPROPERTY(EditAnywhere, Category = "Sandbox|Speed")
    FSpaceShipFlightModel flight_model{};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Speed")
    FSpeedResponses speed_responses{};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Speed")
    FVector velocity;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Speed")
    float target_speed{0.f};

    // Movement - Cruising
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Speed")
    EBoostBrakeState boost_brake_state{EBoostBrakeState::None};

    // Movement - rotation
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Steering")
    FVector2D rotation_input{FVector2D::ZeroVector};

    // Movement - bank/roll
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Steering|Roll")
    float manual_bank_direction{0.f};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    FBarrelRollState roll_state{};

    UPROPERTY()
    float time_since_rotation_input{100.f};

    /* ------------------------------------------------------------------------ */
    /* Combat */
    /* ------------------------------------------------------------------------ */
    // Combat - Laser
    UPROPERTY(EditAnywhere, Category = "Sandbox|Laser")
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Laser")
    EShipLaserMode laser_mode{EShipLaserMode::Single};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Laser")
    float laser_shot_cooldown{0.f};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Laser")
    int32 lasers_fired_this_burst{0};
    int32 lasers_per_burst{3};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Laser")
    AActor* lock_on_target{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Laser")
    ELaserFiringMode laser_firing_mode{ELaserFiringMode::idle};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Laser")
    ETestShipFireRate laser_fire_rate{ETestShipFireRate::Burst3};

    // Combat - Bombs
    UPROPERTY(EditAnywhere, Category = "Sandbox|Bomb")
    int32 bombs{3};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Bomb")
    TWeakObjectPtr<AShipBomb> active_bomb{nullptr};

    /* ------------------------------------------------------------------------------------------ */
    // Health
    /* ------------------------------------------------------------------------------------------ */
    UPROPERTY(EditAnywhere, Category = "Sandbox|Health")
    FShipHealth health{1000};

    // Logging
    UPROPERTY(EditAnywhere, Category = "Sandbox|Logging")
    FActorLoggingConfig log_config{1.f};

#if WITH_EDITORONLY_DATA
    int32 speed_sample_index{0};
    int32 speed_sample_max{0};
    FTimerHandle speed_sample_timer;
    TArray<FVector2d> speed_samples;
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool debug_forward_socket_direction{false};
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool debug_forward_direction{false};
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool debug_lock_on{false};
    UPROPERTY(EditAnywhere, Category = "Debug")
    float debug_lock_on_sphere_radius{1000.f};
#endif
};
