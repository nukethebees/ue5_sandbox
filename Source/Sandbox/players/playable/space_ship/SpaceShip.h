#pragma once

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
    void fire_laser();
    void fire_bomb();
    void upgrade_laser();
    void add_bomb();
    void upgrade_health();
    void add_gold_ring();
    void boost();
    void brake();
  protected:
    void BeginPlay() override;

    void fire_laser_from(UShipLaserConfig const& fire_laser_config, FTransform fire_point);

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UCameraComponent* camera{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    USpringArmComponent* spring_arm{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UStaticMeshComponent* ship_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UBoxComponent* collision_box{nullptr};

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    FVector velocity;

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float target_speed{0.f};

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float cruise_speed{3000.0f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float cruise_acceleration{3000.0f};

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float boost_speed{5000.0f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float boost_acceleration{5000.0f};

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float brake_speed{1000.0f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float brake_acceleration{1000.0f};

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float rotation_speed{0.5f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    FVector2D rotation_input{FVector2D::ZeroVector};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float pitch_angle_max{30.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float pitch_speed{3.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float bank_angle_max{30.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float bank_speed{5.f};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpaceShip")
    EShipLaserMode laser_mode{EShipLaserMode::Single};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float laser_speed{1000.f};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    TSubclassOf<AShipLaser> laser_class;
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UShipLaserConfig* laser_config;
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    UShipLaserConfig* hyper_laser_config;

    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    TSubclassOf<AShipBomb> bomb_class;
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    int32 bombs{3};
    UPROPERTY(VisibleAnywhere, Category = "SpaceShip")
    TWeakObjectPtr<AShipBomb> active_bomb{nullptr};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    int32 gold_rings_collected{0};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float seconds_since_last_log{0};
    UPROPERTY(EditAnywhere, Category = "SpaceShip")
    float seconds_per_log{2.f};
#endif
};
