#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestTurrets.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UBoxComponent;
class UCapsuleComponent;

class AShipLaser;
class UTestTurretsConfig;

UENUM()
enum class ETestTurretState {
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

    UFUNCTION(CallInEditor, Category = "Turret")
    void clear_all_turrets();
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

    void update_target_rotations();
    void integrate_rotations(float const dt);
    void apply_rotations_to_components();

    void create_turrets(int32 const n);

    void capture_turret_layout(int32 const i);
  private:
    // Spawning
#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Turret") void create_turrets_button();
    UFUNCTION(CallInEditor, Category = "Turret") void capture_turret_0_layout_button();
#endif

    // Config
    UPROPERTY(EditAnywhere, Category = "Turret")
    TObjectPtr<UTestTurretsConfig> turret_config{nullptr};

    UPROPERTY(VisibleAnywhere)
    TArray<TObjectPtr<UStaticMeshComponent>> body_meshes{};
    UPROPERTY(VisibleAnywhere)
    TArray<TObjectPtr<UStaticMeshComponent>> cannon_meshes{};

    // Pivots
    UPROPERTY(EditAnywhere)
    TArray<TObjectPtr<USceneComponent>> yaw_pivots;
    UPROPERTY(VisibleAnywhere)
    TArray<TObjectPtr<USceneComponent>> pitch_pivots;

    // Collision
    UPROPERTY(VisibleAnywhere)
    TArray<TObjectPtr<UCapsuleComponent>> collision_shapes{};

    // AI state
    UPROPERTY(VisibleAnywhere, Category = "Turret|AI")
    TArray<ETestTurretState> ai_states{};

    // Rotation
    UPROPERTY(VisibleAnywhere, Category = "Turret|Rotation")
    float pitch_rotation_speed_degrees{90.f};
    UPROPERTY(VisibleAnywhere, Category = "Turret|Rotation")
    float yaw_rotation_speed_degrees{90.f};

    UPROPERTY(VisibleAnywhere, Category = "Turret|Rotation")
    TArray<float> pitch_degrees{};
    UPROPERTY(VisibleAnywhere, Category = "Turret|Rotation")
    TArray<float> yaw_degrees{};

    UPROPERTY(VisibleAnywhere, Category = "Turret|Rotation")
    TArray<float> target_pitch_degrees{};
    UPROPERTY(VisibleAnywhere, Category = "Turret|Rotation")
    TArray<float> target_yaw_degrees{};

// Spawning
#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, Category = "Turret|Laser")
    int32 num_turrets_to_create{1};
#endif
};
