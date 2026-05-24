#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestTurrets.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UCapsuleComponent;

UENUM()
enum class ETestTurretState {
    disabled,
    scanning,
    attacking,
};

UCLASS()
class ATestTurrets : public AActor {
  public:
    GENERATED_BODY()

    ATestTurrets();

    void Tick(float dt) override;

    auto num_turrets() const noexcept -> int32;
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

    void integrate_rotations(float dt);
    void apply_rotations_to_components();

    UPROPERTY(VisibleAnywhere, Category = "Turret")
    TArray<TObjectPtr<UStaticMeshComponent>> body_meshes{};
    UPROPERTY(VisibleAnywhere, Category = "Turret")
    TArray<TObjectPtr<UStaticMeshComponent>> barrel_meshes{};
    UPROPERTY(VisibleAnywhere, Category = "Turret")
    TArray<TObjectPtr<UCapsuleComponent>> collision_shapes{};

    // AI state
    UPROPERTY(VisibleAnywhere, Category = "Turret")
    TArray<ETestTurretState> ai_states{};

    // Rotation
    UPROPERTY(VisibleAnywhere, Category = "Turret")
    float pitch_rotation_speed_degrees{90.f};
    UPROPERTY(VisibleAnywhere, Category = "Turret")
    float yaw_rotation_speed_degrees{90.f};

    UPROPERTY(VisibleAnywhere, Category = "Turret")
    TArray<float> pitch_degrees{};
    UPROPERTY(VisibleAnywhere, Category = "Turret")
    TArray<float> yaw_degrees{};

    UPROPERTY(VisibleAnywhere, Category = "Turret")
    TArray<float> target_pitch_degrees{};
    UPROPERTY(VisibleAnywhere, Category = "Turret")
    TArray<float> target_yaw_degrees{};
};
