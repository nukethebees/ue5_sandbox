#pragma once

#include "Sandbox/logging/ActorLoggingConfig.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestTurrets.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UBoxComponent;
class UCapsuleComponent;
class UArrowComponent;

class AShipLaser;
class UTestTurretsConfig;

UENUM()
enum class ETestTurretState {
    scanning,
    attacking,
};

USTRUCT()
struct FTestTurretsSearchData {
    GENERATED_BODY()

    auto num_turrets() const -> int32;
    auto num_turrets_to_move() const -> int32;
    void add_uninitialised(int32 const n);
    void reset();

    void rotate_by(float const dt, float const r);
  private:
    static void rotate_by(float* yaw_degrees, int32 const n, float dt, float const r);
  public:
    // Visuals
    UPROPERTY(EditAnywhere)
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

    // Location
    UPROPERTY()
    TArray<float> location_xs;
    UPROPERTY()
    TArray<float> location_ys;
    UPROPERTY()
    TArray<float> location_zs;

    // Rotation
    UPROPERTY(VisibleAnywhere, Category = "Turret|Rotation")
    TArray<float> yaw_degrees{};

    // Health
    UPROPERTY()
    TArray<int32> healths;

    // Transition
    UPROPERTY()
    TArray<int32> to_attack;
    UPROPERTY()
    TArray<TWeakObjectPtr<AActor>> attack_targets;
};

USTRUCT()
struct FTestTurretsAttackData {
    GENERATED_BODY()

    auto num_turrets() const -> int32;
    auto num_turrets_to_move() const -> int32;
    void reset();

    // Visuals
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> body_meshes{};
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> cannon_meshes{};

    // Pivots
    UPROPERTY()
    TArray<TObjectPtr<USceneComponent>> yaw_pivots;
    UPROPERTY()
    TArray<TObjectPtr<USceneComponent>> pitch_pivots;

    // Collision
    UPROPERTY()
    TArray<TObjectPtr<UCapsuleComponent>> collision_shapes{};

    // Location
    UPROPERTY()
    TArray<float> location_xs;
    UPROPERTY()
    TArray<float> location_ys;
    UPROPERTY()
    TArray<float> location_zs;

    // Rotation
    UPROPERTY()
    TArray<float> pitch_degrees{};
    UPROPERTY()
    TArray<float> yaw_degrees{};

    UPROPERTY()
    TArray<float> target_pitch_degrees{};
    UPROPERTY()
    TArray<float> target_yaw_degrees{};

    // Targets
    UPROPERTY(VisibleAnywhere)
    TArray<TWeakObjectPtr<AActor>> targets;

    // Transition
    UPROPERTY()
    TArray<int32> to_search;

    // Health
    UPROPERTY()
    TArray<int32> healths;
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

    void update_locations_from_components();

    void update_target_rotations();
    void integrate_rotations(float const dt);
    void apply_rotations_to_components();

    void create_turrets(int32 const n);
    void configure_collision(UStaticMeshComponent& sm);

    void perform_search();
    void change_turret_state();

#if WITH_EDITOR
    void capture_turret_layout(int32 const i);
#endif

    // Spawning
#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Turret")
    void create_turrets_button();
    UFUNCTION(CallInEditor, Category = "Turret")
    void capture_turret_0_layout_button();
#endif

    // Debugging
#if WITH_EDITOR
    void draw_debug_shapes();
    void draw_searching_debug_shapes();
    void draw_attacking_debug_shapes();
#endif

    // Config
    UPROPERTY(EditAnywhere, Category = "Turret")
    TObjectPtr<UTestTurretsConfig> turret_config{nullptr};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Turret")
    TObjectPtr<UArrowComponent> fire_point_marker;
#endif

    // Rotation
    UPROPERTY(VisibleAnywhere, Category = "Turret|Rotation")
    float pitch_rotation_speed_degrees{90.f};
    UPROPERTY(VisibleAnywhere, Category = "Turret|Rotation")
    float yaw_rotation_speed_degrees{90.f};

    // Searching
    UPROPERTY(VisibleAnywhere, Category = "Turret")
    FTestTurretsSearchData searching{};

    // Attacking
    UPROPERTY(VisibleAnywhere, Category = "Turret")
    FTestTurretsAttackData attacking{};

// Spawning
#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Turret|Spawning")
    int32 num_turrets_to_create{1};
#endif

    // Debugging
    UPROPERTY(EditAnywhere, Category = "Turret|Logging")
    FActorLoggingConfig log_config{1.f};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Turret|Debugging")
    bool debug_shapes_enabled{false};
#endif
};
