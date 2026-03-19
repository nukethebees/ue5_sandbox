#pragma once

#include "Sandbox/combat/weapons/ShipProjectileType.h"
#include "Sandbox/health/ShipHealthComponent.h"
#include "Sandbox/players/DamageableShip.h"
#include "Sandbox/players/ShipLaserMode.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Math/Color.h"

#include <span>

#include "SpaceShip.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UNiagaraSystem;
class UNiagaraComponent;

class AShipLaser;
class UShipLaserConfig;
class AShipBomb;
class UShipHealthComponent;

DECLARE_DELEGATE_OneParam(FOnShipSpeedChanged, float);
DECLARE_DELEGATE_OneParam(FOnShipEnergyChanged, float);
DECLARE_DELEGATE_OneParam(FOnShipBombsChanged, int32);
DECLARE_DELEGATE_OneParam(FOnShipGoldRingsChanged, int32);
DECLARE_DELEGATE_OneParam(FOnShipPointsChanged, int32);
DECLARE_DELEGATE_OneParam(FOnLivesChanged, int32);
DECLARE_DELEGATE_TwoParams(FOnSpeedSampled, std::span<FVector2d>, int32);

UENUM()
enum class EBoostBrakeState : uint8 { None, Boost, Brake };

USTRUCT(BlueprintType)
struct FSpeedResponse {
    GENERATED_BODY()

    FSpeedResponse() = default;
    FSpeedResponse(float settling_time, float damping_ratio)
        : settling_time(settling_time)
        , damping_ratio(damping_ratio) {}

    auto tau() const { return settling_time / 5.f; }
    auto natural_angular_frequency() const { return 1 / tau(); }

    UPROPERTY(EditAnywhere)
    float settling_time{3.f};
    UPROPERTY(EditAnywhere)
    float damping_ratio{0.5f};
};

USTRUCT(BlueprintType)
struct FSpeedResponses {
    GENERATED_BODY()

    FSpeedResponses() = default;

    UPROPERTY(EditAnywhere)
    FSpeedResponse boost{};
    UPROPERTY(EditAnywhere)
    FSpeedResponse brake{};
    UPROPERTY(EditAnywhere)
    FSpeedResponse slowing_to_cruise{};
    UPROPERTY(EditAnywhere)
    FSpeedResponse accelerating_to_cruise{};
};

USTRUCT(BlueprintType)
struct FSpaceShipFlightModel {
    GENERATED_BODY()

    FSpaceShipFlightModel() = default;
    FSpaceShipFlightModel(FSpeedResponse sr)
        : response(sr) {}

    auto step_size() const { return target_speed - old_speed; }

    auto calculate_dy(float t) const -> float;
    auto update_y(float dt) -> float;
    auto set_new_impulse(FSpeedResponse sr, float old_s, float target_s);
  protected:
    UPROPERTY(VisibleAnywhere)
    FSpeedResponse response{};
    UPROPERTY(VisibleAnywhere)
    float time{0.f};
    UPROPERTY(VisibleAnywhere)
    float old_speed{0.f};
    UPROPERTY(VisibleAnywhere)
    float target_speed{0.f};
    UPROPERTY(VisibleAnywhere)
    float alpha{0.f};
    UPROPERTY(VisibleAnywhere)
    float wn{0.f};
    UPROPERTY(VisibleAnywhere)
    float wd{0.f};
    UPROPERTY(VisibleAnywhere)
    float z_wn{0.f};
    // Harmonic addition theorem
    // sin(x) + b*cos(x) -> c * cos(x + delta)
    UPROPERTY(VisibleAnywhere)
    float delta{0.f};
    UPROPERTY(VisibleAnywhere)
    float c{0.f};

#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, Category = "Debug")
    mutable float h_dbg{0.f};
    UPROPERTY(VisibleAnywhere, Category = "Debug")
    float step_size_original_dbg{0.f};
    UPROPERTY(VisibleAnywhere, Category = "Debug")
    float step_size_dbg{0.f};
    UPROPERTY(VisibleAnywhere, Category = "Debug")
    float dy_dbg{0.f};
#endif
};

USTRUCT(BlueprintType)
struct FBarrelRoll {
    GENERATED_BODY()

    FBarrelRoll() = default;

    auto is_rolling() const { return time_remaining > 0.f; }
    auto can_roll() const { return time_remaining < (-1.f * roll_cooldown); }

    UPROPERTY(EditAnywhere)
    float roll_speed{90.f};
    UPROPERTY(EditAnywhere)
    float roll_duration{2.5f};
    UPROPERTY(EditAnywhere)
    float roll_cooldown{0.5f};

    UPROPERTY(VisibleAnywhere)
    float direction{0.f};
    UPROPERTY(VisibleAnywhere)
    float time_remaining{0.f};
};

UCLASS()
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

    void turn(FVector2D direction);
    void start_boost();
    void stop_boost();
    void start_brake();
    void stop_brake();
    auto get_velocity() const { return velocity; }
    auto get_speed() const { return get_velocity().Size(); }
    void roll(float direction);
    void barrel_roll(float direction);

    void fire_laser();
    void fire_bomb();
    void upgrade_laser();
    void add_bomb();
    auto get_bombs() const { return bombs; }

    void upgrade_max_health();
    void add_health(int32 added_health);
    auto get_health_info() const { return health->get_health_info(); }
    void add_gold_ring();
    auto get_gold_rings() const { return gold_rings_collected; }

    auto get_points() const { return points; }
    void record_kills(int32 kills);
    auto get_lives() const { return lives; }
    void add_life();
    auto energy_is_full() const { return thrust_energy == thrust_energy_max; }

    static constexpr auto tick_clamp(auto value, auto delta_time, auto abs_max_value) {
        return FMath::Clamp(value * delta_time, -abs_max_value, abs_max_value);
    }
    static constexpr auto clamp(auto value, auto abs_max_value) {
        return FMath::Clamp(value, -abs_max_value, abs_max_value);
    }

    auto get_ship_forward_vector() const -> FVector;
    auto get_middle_socket() const -> FTransform;

    // IDamageableShip
    auto apply_damage(int32 damage, AActor const& instigator) -> FShipDamageResult override;

    FOnShipSpeedChanged on_speed_changed;
    auto get_on_health_changed_delegate() -> FOnShipHealthChanged&;
    FOnShipEnergyChanged on_energy_changed;
    FOnShipBombsChanged on_bombs_changed;
    FOnShipGoldRingsChanged on_gold_rings_changed;
    FOnShipPointsChanged on_points_changed;
    FOnLivesChanged on_lives_changed;

#if WITH_EDITORONLY_DATA
    FOnSpeedSampled on_speed_sampled;
#endif
  protected:
    void BeginPlay() override;

    void set(EBoostBrakeState s);
    void fire_laser_from(UShipLaserConfig const& fire_laser_config, FTransform fire_point);
    void subtract_bomb();
    void update_boost_brake(this ASpaceShip& self, float dt);
    void update_actor_rotation(this ASpaceShip& self, float dt);
    void update_visual_orientation(this ASpaceShip& self, float dt);
    void integrate_velocity(this ASpaceShip& self, float dt);

    void add_points(int32 x);

    auto get_middle_socket(UStaticMeshComponent const& m) const -> FTransform;

#if WITH_EDITOR
    auto can_log() const -> bool { return seconds_since_last_log >= seconds_per_log; }
    void sample_speed();
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

    UPROPERTY(EditAnywhere, Category = "SpaceShip|Speed")
    FSpaceShipFlightModel flight_model{};
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
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Steering")
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpaceShip|Laser")
    EShipLaserMode laser_mode{EShipLaserMode::Single};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    float laser_speed{1000.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    TSubclassOf<AShipLaser> laser_class;
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    UShipLaserConfig* laser_config;
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Laser")
    UShipLaserConfig* hyper_laser_config;

    UPROPERTY(EditAnywhere, Category = "SpaceShip|Bomb")
    TSubclassOf<AShipBomb> bomb_class;
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Bomb")
    int32 bombs{3};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip|Bomb")
    TWeakObjectPtr<AShipBomb> active_bomb{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Health")
    int32 gold_rings_collected{0};

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    int32 points{0};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Health")
    int32 lives{3};
    UPROPERTY(EditAnywhere, Category = "SpaceShip|Health")
    UShipHealthComponent* health{nullptr};

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
#endif
};
