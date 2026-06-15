#pragma once

#include <Sandbox/health/ShipHealthComponent.h>
#include <Sandbox/logging/ActorLoggingConfig.h>
#include <Sandbox/misc/learning/TestEntityOwnerId.h>
#include <Sandbox/misc/learning/TestEntityRegistryData.h>
#include <Sandbox/misc/learning/TestEntityUniqueId.h>
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

    // Entity data
    void set_owner_id(TestEntityOwnerId const new_owner_id);
    auto get_owner_id() const -> TestEntityOwnerId;

    // Movement
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
    auto energy_is_full() const { return thrust_energy == thrust_energy_max; }

    // Combat
    auto get_lock_on_target() const -> AActor const* { return lock_on_target; }
    // Combat - laser
    void start_fire_laser();
    void stop_fire_laser();
    void upgrade_laser();
    // Combat - bomb
    void fire_bomb();
    void add_bomb();
    auto get_bombs() const { return bombs; }

    // Health
    void add_health(int32 added_health);
    auto get_health_info() const -> FShipHealth { return health; }
    // Lives
    auto get_lives() const { return lives; }
    void add_life();

    // Points
    auto get_points() const { return points; }
    void record_kills(int32 kills);

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
    FOnShipGoldRingsChanged on_gold_rings_changed;
    FOnShipPointsChanged on_points_changed;
    FOnLivesChanged on_lives_changed;
    FOnLaserModeChanged on_laser_mode_changed;
    FOnLockOnAcquired on_lock_on_acquired;

#if WITH_EDITORONLY_DATA
    FOnSpeedSampled on_speed_sampled;
#endif
  protected:
    // Entity data
    auto get_entity_update_data() const -> FTestEntityRegistryEntityData;

    // Movement
    auto GetVelocity() const -> FVector override;
    void set(EBoostBrakeState s);
    void update_boost_brake(this ATestSpaceShip& self, float dt);
    void integrate_velocity(this ATestSpaceShip& self, float dt);

    // Combat
    void set_lock_on_target(AActor* target);
    // Combat - laser
    void set_laser_mode(ELaserFiringMode laser_mode);
    void update_laser_firing();
    void fire_laser();
    void fire_lasers_from(UShipLaserConfig const& fire_laser_config,
                          TConstArrayView<FTransform> const fire_points);
    // Combat - bomb
    void subtract_bomb();
    // Combat - homing laser
    void fire_homing_laser();

    // Visuals
    void update_actor_rotation(this ATestSpaceShip& self, float dt);
    void update_visual_orientation(this ATestSpaceShip& self, float dt);

    // Scoring
    void add_points(int32 x);

    // Mesh
    auto get_middle_socket(UStaticMeshComponent const& m) const -> FTransform;

    // Debugging
    void draw_debug_shapes();
#if WITH_EDITOR
    void sample_speed();
#endif

    // ------------------------------------------------------------------------
    // Entity data
    // ------------------------------------------------------------------------
    TestEntityOwnerId owner_id{};
    TestEntityUniqueId unique_entity_id;

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    FGenerationIndex entity_index{};

    // ------------------------------------------------------------------------
    // Collision
    // ------------------------------------------------------------------------
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UBoxComponent* collision_box{nullptr};

    // ------------------------------------------------------------------------
    // Visuals
    // ------------------------------------------------------------------------
    // Camera
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UCameraComponent* camera{nullptr};

    // Ship
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UStaticMeshComponent* ship_mesh{nullptr};

    // Visuals - engine
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Niagara")
    UNiagaraComponent* boost_pulse{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Niagara")
    UNiagaraComponent* boost_engine_effect{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Niagara")
    float boost_effect_colour_intensity{75.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    FLinearColor engine_colour{FLinearColor::Blue};

    // ------------------------------------------------------------------------
    // Energy
    // ------------------------------------------------------------------------
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Energy")
    float thrust_energy_max{1.f};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Energy")
    float thrust_energy{1.f};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Energy")
    float thrust_change_rate{0.f};

    // ------------------------------------------------------------------------
    // Movement
    // ------------------------------------------------------------------------

    // Movement - Speed
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    FSpaceShipFlightModel flight_model{};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    FSpeedResponses speed_responses{};

    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Speed")
    FVector velocity;
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Speed")
    float target_speed{0.f};

    // Movement - Cruising
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    float cruise_speed{12000.0f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    float thrust_recharge_time{7.f};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Speed")
    EBoostBrakeState boost_brake_state{EBoostBrakeState::None};

    // Movement - Boosting
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    float boost_speed{30000.0f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    float boost_depletion_time{4.f};

    // Movement - Braking
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    float brake_speed{1000.0f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    float brake_depletion_time{6.f};

    // Movement - rotation
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering")
    float rotation_speed{60.f};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Steering")
    FVector2D rotation_input{FVector2D::ZeroVector};

    // Movement - pitch
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering|Pitch")
    float pitch_angle_max{30.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering|Pitch")
    float pitch_speed{3.f};

    // Movement - yaw
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering|Yaw")
    float yaw_angle_max{30.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering|Yaw")
    float yaw_speed{3.f};

    // Movement - bank/roll
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering|Roll")
    float turn_bank_angle_max{30.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering|Roll")
    float turn_bank_speed{2.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering|Roll")
    float manual_bank_direction{0.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering|Roll")
    float manual_bank_angle_max{90.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering|Roll")
    float manual_bank_speed{5.f};

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    FBarrelRoll roll_state{};

    // ------------------------------------------------------------------------
    // Combat
    // ------------------------------------------------------------------------

    // Combat - Laser
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    TObjectPtr<ATestLasers> laser_actor{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    EShipLaserMode laser_mode{EShipLaserMode::Single};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    float laser_speed{1000.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    float laser_firing_period{0.15f};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Laser")
    float laser_shot_cooldown{0.f};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Laser")
    int32 lasers_fired_this_burst{0};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    int32 lasers_per_burst{3};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    float laser_lock_on_transition_delay{1.f};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Laser")
    AActor* lock_on_target{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    float laser_lock_on_distance{10000.f};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Laser")
    ELaserFiringMode laser_firing_mode{ELaserFiringMode::idle};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    TSubclassOf<AShipLaser> laser_class;
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    TSubclassOf<AShipHomingLaser> homing_laser_class;
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    UShipLaserConfig* laser_config;
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    UShipLaserConfig* hyper_laser_config;

    // Combat - Bombs
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Bomb")
    TSubclassOf<AShipBomb> bomb_class;
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Bomb")
    int32 bombs{3};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Bomb")
    TWeakObjectPtr<AShipBomb> active_bomb{nullptr};

    // ------------------------------------------------------------------------
    // Misc
    // ------------------------------------------------------------------------

    // Points
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    int32 points{0};

    // Health
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Health")
    int32 gold_rings_collected{0};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Health")
    FShipHealth health{1000000};

    // Lives
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Health")
    int32 lives{3};

    // Logging
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Logging")
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
