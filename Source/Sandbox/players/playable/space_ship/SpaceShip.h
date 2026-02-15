#pragma once

#include "Sandbox/health/ShipHealthComponent.h"
#include "Sandbox/players/playable/space_ship/ShipLaserMode.h"

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "SpaceShip.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UBoxComponent;

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

UENUM()
enum class EBoostBrakeState : uint8 { None, Boost, Brake };

UCLASS()
class ASpaceShip : public APawn {
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

    void fire_laser();
    void fire_bomb();
    void upgrade_laser();
    void add_bomb();
    auto get_bombs() const { return bombs; }

    void upgrade_health();
    void add_gold_ring();
    auto get_gold_rings() const { return gold_rings_collected; }

    void add_points(int32 x);
    auto get_points() const { return points; }
    auto get_lives() const { return lives; }
    auto get_health_info() const { return health->get_health_info(); }
    auto energy_is_full() const { return thrust_energy == thrust_energy_max; }

    static constexpr auto tick_clamp(auto value, auto delta_time, auto abs_max_value) {
        return FMath::Clamp(value * delta_time, -abs_max_value, abs_max_value);
    }

    FOnShipSpeedChanged on_speed_changed;
    auto get_on_health_changed_delegate() -> FOnShipHealthChanged&;
    FOnShipEnergyChanged on_energy_changed;
    FOnShipBombsChanged on_bombs_changed;
    FOnShipGoldRingsChanged on_gold_rings_changed;
    FOnShipPointsChanged on_points_changed;
    FOnLivesChanged on_lives_changed;
  protected:
    void BeginPlay() override;

    void fire_laser_from(UShipLaserConfig const& fire_laser_config, FTransform fire_point);
    void subtract_bomb();
    void update_boost_brake(this auto& self, float dt);
    void update_actor_rotation(this auto& self, float dt);
    void update_visual_orientation(this auto& self, float dt);
    void integrate_velocity(this auto& self, float dt);

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UCameraComponent* camera{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    USpringArmComponent* spring_arm{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UStaticMeshComponent* ship_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UBoxComponent* collision_box{nullptr};

    UPROPERTY(EditAnywhere, Category = "SpaceShip | Energy")
    float thrust_energy_max{100.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Energy")
    float thrust_energy{100.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Energy")
    float boost_depletion_rate{50.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Energy")
    float brake_depletion_rate{30.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Energy")
    float thrust_recharge_rate{20.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Energy")
    EBoostBrakeState boost_brake_state{EBoostBrakeState::None};

    UPROPERTY(EditAnywhere, Category = "SpaceShip | Speed")
    FVector velocity;
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Speed")
    float target_speed{0.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Speed")
    float max_acceleration{0.f};

    UPROPERTY(EditAnywhere, Category = "SpaceShip | Speed")
    float cruise_speed{3000.0f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Speed")
    float cruise_acceleration{3000.0f};

    UPROPERTY(EditAnywhere, Category = "SpaceShip | Speed")
    float boost_speed{5000.0f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Speed")
    float boost_acceleration{5000.0f};

    UPROPERTY(EditAnywhere, Category = "SpaceShip | Speed")
    float brake_speed{1000.0f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Speed")
    float brake_acceleration{1000.0f};

    UPROPERTY(EditAnywhere, Category = "SpaceShip | Steering")
    float rotation_speed{0.5f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Steering")
    FVector2D rotation_input{FVector2D::ZeroVector};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Steering")
    float pitch_angle_max{30.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Steering")
    float pitch_speed{3.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Steering")
    float bank_angle_max{30.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Steering")
    float bank_speed{5.f};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpaceShip | Laser")
    EShipLaserMode laser_mode{EShipLaserMode::Single};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Laser")
    float laser_speed{1000.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Laser")
    TSubclassOf<AShipLaser> laser_class;
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Laser")
    UShipLaserConfig* laser_config;
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Laser")
    UShipLaserConfig* hyper_laser_config;

    UPROPERTY(EditAnywhere, Category = "SpaceShip | Bomb")
    TSubclassOf<AShipBomb> bomb_class;
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Bomb")
    int32 bombs{3};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip | Bomb")
    TWeakObjectPtr<AShipBomb> active_bomb{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Health")
    int32 gold_rings_collected{0};

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    int32 points{0};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Health")
    int32 lives{3};
    UPROPERTY(EditAnywhere, Category = "SpaceShip | Health")
    UShipHealthComponent* health{nullptr};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float seconds_since_last_log{0};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float seconds_per_log{2.f};
#endif
};
