// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/health/data/HealthChange.h"
#include "Sandbox/players/common/enums/TeamID.h"

#include "FloorTurret.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class ABulletActor;
class UArrowComponent;

UENUM(BlueprintType)
enum class EFloorTurretState : uint8 {
    Disabled UMETA(DisplayName = "Disabled"),
    Watching UMETA(DisplayName = "Watching"),
    Attacking UMETA(DisplayName = "Attacking"),
};

UENUM(BlueprintType)
enum class EFloorTurretScanDirection : uint8 {
    clockwise UMETA(DisplayName = "clockwise"),
    anticlockwise UMETA(DisplayName = "anticlockwise"),
};

USTRUCT(BlueprintType)
struct FFloorTurretBulletConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Turret")
    TSubclassOf<ABulletActor> bullet_actor_class;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret")
    FHealthChange bullet_damage{5.0f, EHealthChangeType::Damage};
    UPROPERTY(EditAnywhere, Category = "Turret")
    float bullet_speed{1000.0f};
    UPROPERTY(EditAnywhere, Category = "Turret")
    float fire_rate{10.0f};
};

USTRUCT(BlueprintType)
struct FFloorTurretAimConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Turret")
    float watching_rotation_speed_degrees_per_second{45.0f};
    UPROPERTY(EditAnywhere, Category = "Turret")
    float attacking_rotation_speed_degrees_per_second{95.0f};
    // Angles are half of the full rotation
    UPROPERTY(EditAnywhere, Category = "Turret")
    float rotation_degrees{90.0f};
    UPROPERTY(EditAnywhere, Category = "Turret")
    float watching_cone_degrees{60.0f};
    UPROPERTY(EditAnywhere, Category = "Turret")
    float tracking_cone_degrees{100.0f};
    UPROPERTY(EditAnywhere, Category = "Turret")
    float watching_cone_radius{500.0f};
};

USTRUCT(BlueprintType)
struct FFloorTurretAimState {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Turret")
    float camera_rotation_angle{0.0f};
    UPROPERTY(EditAnywhere, Category = "Turret")
    float muzzle_rotation_angle{0.0f};
    UPROPERTY(EditAnywhere, Category = "Turret")
    EFloorTurretScanDirection scan_direction{EFloorTurretScanDirection::clockwise};
};

USTRUCT(BlueprintType)
struct FFloorTurretState {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Turret")
    ETeamID team_id{ETeamID::Enemy};
    UPROPERTY(EditAnywhere, Category = "Turret")
    EFloorTurretState operating_state{EFloorTurretState::Watching};
    UPROPERTY(EditAnywhere, Category = "Turret")
    AActor* current_target{nullptr};
};

UCLASS()
class SANDBOX_API AFloorTurret : public AActor {
    GENERATED_BODY()
  public:
    AFloorTurret();

    virtual void Tick(float dt) override;

    void set_state(EFloorTurretState new_state);
    auto get_state() const { return state; }

    bool within_vision_cone(AActor& target, float cone_degrees) const;
    bool is_enemy(AActor& target) const;
  protected:
    virtual void BeginPlay() override;

    void handle_watching_state(float dt);
    void handle_attacking_state();
    void fire_bullet();

    UPROPERTY(EditAnywhere, Category = "Turret")
    UStaticMeshComponent* base_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Turret")
    USceneComponent* pivot{nullptr};
    UPROPERTY(EditAnywhere, Category = "Turret")
    USceneComponent* camera_pivot{nullptr};
    UPROPERTY(EditAnywhere, Category = "Turret")
    UStaticMeshComponent* cannon_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Turret")
    UStaticMeshComponent* camera_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Turret")
    UArrowComponent* muzzle_point{nullptr};

    UPROPERTY(EditAnywhere, Category = "Turret")
    FFloorTurretBulletConfig bullet_config;
    UPROPERTY(EditAnywhere, Category = "Turret")
    FFloorTurretAimConfig aim_config;
    UPROPERTY(EditAnywhere, Category = "Turret")
    FFloorTurretAimState aim_state;
    UPROPERTY(EditAnywhere, Category = "Turret")
    FFloorTurretState state;
};
