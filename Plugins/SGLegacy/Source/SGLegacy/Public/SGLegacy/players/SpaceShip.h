#pragma once

#include "SandboxGameShared/players/SpeedResponse.h"
#include "SpaceGame/combat/DamageableShip.h"
#include "SpaceGame/ships/common/BarrelRoll.h"
#include "SpaceGame/ships/common/LaserFiringState.h"
#include "SpaceGame/ships/common/ShipHealthComponent.h"
#include "SpaceGame/ships/common/ShipLaserMode.h"
#include "SpaceGame/ships/common/SpaceShipCommon.h"
#include "SpaceGame/ships/common/SpaceShipFlightModel.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Math/Color.h"

#include "SpaceShip.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UNiagaraSystem;
class UNiagaraComponent;

class AShipLaser;
class UShipLaserConfig;
class UShipHealthComponent;

UCLASS(MinimalAPI)
class ASpaceShip
    : public APawn
    , public IDamageableShip {
    GENERATED_BODY()
  public:
    struct Sockets {
        inline static FName const left{"Left"};
        inline static FName const right{"Right"};
        inline static FName const middle{"Middle"};
    };

    ASpaceShip();

    void Tick(float dt) override;

    SGLEGACY_API void turn(FVector2D direction);
    SGLEGACY_API void start_boost();
    SGLEGACY_API void stop_boost();
    SGLEGACY_API void start_brake();
    SGLEGACY_API void stop_brake();
    auto get_velocity() const { return velocity; }
    auto get_speed() const { return get_velocity().Size(); }
    SGLEGACY_API void roll(float direction);
    SGLEGACY_API void barrel_roll(float direction);

    SGLEGACY_API void start_fire_laser();
    SGLEGACY_API void stop_fire_laser();
    SGLEGACY_API void upgrade_laser();
    auto get_lock_on_target() const -> AActor const* { return lock_on_target; }

    void upgrade_max_health();
    SGLEGACY_API void add_health(int32 added_health);
    auto get_health_info() const -> FShipHealth { return health->get_health_info(); }
    SGLEGACY_API void add_gold_ring();
    auto get_gold_rings() const { return gold_rings_collected; }

    auto get_points() const { return points; }
    void record_kills(int32 kills);
    auto get_lives() const { return lives; }
    SGLEGACY_API void add_life();
    auto energy_is_full() const { return thrust_energy == thrust_energy_max; }

    static constexpr auto tick_clamp(auto value, auto delta_time, auto abs_max_value) {
        return FMath::Clamp(value * delta_time, -abs_max_value, abs_max_value);
    }
    static constexpr auto clamp(auto value, auto abs_max_value) {
        return FMath::Clamp(value, -abs_max_value, abs_max_value);
    }

    auto get_ship_forward_vector() const -> FVector;
    SGLEGACY_API auto get_middle_socket() const -> FTransform;

    // IDamageableShip
    auto apply_damage(ShipDamageContext context) -> FShipDamageResult override;
    auto get_on_killed_delegate() -> FOnKilled& override { return on_killed; }

    FOnShipSpeedChanged on_speed_changed;
    SGLEGACY_API auto get_on_health_changed_delegate() -> FOnShipHealthChanged&;
    FOnShipEnergyChanged on_energy_changed;
    FOnShipGoldRingsChanged on_gold_rings_changed;
    FOnShipPointsChanged on_points_changed;
    FOnLivesChanged on_lives_changed;
    FOnLaserModeChanged on_laser_mode_changed;
    FOnLockOnAcquired on_lock_on_acquired;

#if WITH_EDITORONLY_DATA
    FOnSpeedSampled on_speed_sampled;
#endif
  protected:
    void BeginPlay() override;

    void set(EBoostBrakeState s);
    void set_laser_mode(ELaserFiringState laser_mode);
    void update_boost_brake(this ASpaceShip& self, float dt);
    void update_actor_rotation(this ASpaceShip& self, float dt);
    void update_visual_orientation(this ASpaceShip& self, float dt);
    void integrate_velocity(this ASpaceShip& self, float dt);
    void update_laser_firing(float dt);

    void fire_laser();
    void fire_laser_from(UShipLaserConfig const& fire_laser_config, FTransform fire_point);

    void add_points(int32 x);
    void record_laser_kill();

    auto get_middle_socket(UStaticMeshComponent const& m) const -> FTransform;
    void set_lock_on_target(AActor* target);

#if WITH_EDITOR
    auto can_log() const -> bool { return seconds_since_last_log >= seconds_per_log; }
    void sample_speed();
    void tick_debugs(float dt);
#endif

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UCameraComponent* camera{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UStaticMeshComponent* ship_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UBoxComponent* collision_box{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Niagara")
    UNiagaraComponent* boost_pulse{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Niagara")
    UNiagaraComponent* boost_engine_effect{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Niagara")
    float boost_effect_colour_intensity{75.f};

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    FLinearColor engine_colour{FLinearColor::Blue};

    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Energy")
    float thrust_energy_max{1.f};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Energy")
    float thrust_energy{1.f};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Energy")
    float thrust_change_rate{0.f};

    TSpaceShipFlightModel<float> flight_model{};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    FSpeedResponses speed_responses{};

    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Speed")
    FVector velocity;
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Speed")
    float target_speed{0.f};

    // Cruising
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    float cruise_speed{12000.0f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    float thrust_recharge_time{7.f};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Speed")
    EBoostBrakeState boost_brake_state{EBoostBrakeState::None};
    // Boosting
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    float boost_speed{30000.0f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    float boost_depletion_time{4.f};
    // Braking
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    float brake_speed{1000.0f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    float brake_depletion_time{6.f};

    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering")
    float rotation_speed{0.5f};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Steering")
    FVector2D rotation_input{FVector2D::ZeroVector};

    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering|Pitch")
    float pitch_angle_max{30.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering|Pitch")
    float pitch_speed{3.f};

    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering|Yaw")
    float yaw_angle_max{30.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering|Yaw")
    float yaw_speed{3.f};

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

    // Laser
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpaceShip|Laser")
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
    ELaserFiringState laser_firing_mode{ELaserFiringState::idle};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    TSubclassOf<AShipLaser> laser_class;
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    UShipLaserConfig* laser_config;
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    UShipLaserConfig* hyper_laser_config;

    // Points
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    int32 points{0};

    // Health/lives
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Health")
    int32 gold_rings_collected{0};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Health")
    int32 lives{3};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Health")
    UShipHealthComponent* health{nullptr};
    FOnKilled on_killed;

#if WITH_EDITORONLY_DATA
    int32 speed_sample_index{0};
    int32 speed_sample_max{0};
    FTimerHandle speed_sample_timer;
    TArray<FVector2d> speed_samples;
    UPROPERTY(EditAnywhere, Category = "Debug")
    float seconds_since_last_log{0};
    UPROPERTY(EditAnywhere, Category = "Debug")
    float seconds_per_log{0.75f};
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
