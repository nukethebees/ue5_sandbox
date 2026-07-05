#pragma once

#include "Sandbox/combat/BurstFire.h"
#include "Sandbox/logging/ActorLoggingConfig.h"
#include "Sandbox/misc/learning/TestMaterialConfig.h"
#include "Sandbox/players/VisionConfig.h"
#include "Sandbox/utilities/DrawDebugConfig.h"
#include "TestFlyBase.h"

#include "CoreMinimal.h"

#include "TestFlySeekDestroy.generated.h"

UENUM()
enum class ETestFlySeekDestroyState : uint8 {
    searching,
    chasing,
    attacking,
};

class ATestVolume;
class AShipLaser;

USTRUCT()
struct FTestFlySeekDestroySearchState {
    GENERATED_BODY()

    // Visuals
    UPROPERTY(EditAnywhere)
    FTestMaterialConfig material_config;
    UPROPERTY(EditAnywhere, Category = "Debug")
    FDrawDebugConfig draw_config;
    UPROPERTY(EditAnywhere, Category = "Debug")
    float destination_sphere_radius{100.f};

    // Movement
    UPROPERTY(EditAnywhere)
    float speed{1000.f};

    // Search
    UPROPERTY(VisibleAnywhere)
    FVector destination{FVector::ZeroVector};
    UPROPERTY(EditAnywhere)
    TObjectPtr<ATestVolume> search_volume{nullptr};
    UPROPERTY(EditAnywhere)
    float min_distance_to_new_point{5000.f};
    UPROPERTY(EditAnywhere)
    float acceptance_radius{500.f};
};

USTRUCT()
struct FTestFlySeekDestroyChaseState {
    GENERATED_BODY()

    // Visuals
    UPROPERTY(EditAnywhere)
    FTestMaterialConfig material_config;
    UPROPERTY(EditAnywhere)
    FDrawDebugConfig draw_config;

    // Movement
    UPROPERTY(EditAnywhere)
    float speed{5000.f};

    UPROPERTY(EditAnywhere)
    float acceptance_radius{2000.f};
};

USTRUCT()
struct FTestFlySeekDestroyAttackState {
    GENERATED_BODY()

    // Visuals
    UPROPERTY(EditAnywhere)
    FTestMaterialConfig material_config;

    // Movement
    UPROPERTY(EditAnywhere)
    float speed{5000.f};

    // Firing
    UPROPERTY(EditAnywhere)
    FBurstFire burst{};
    UPROPERTY(EditAnywhere)
    TSubclassOf<AShipLaser> laser_class;
    UPROPERTY(EditAnywhere)
    float fire_point_distance{100.f};
    UPROPERTY(EditAnywhere)
    float max_fire_angle_degrees{3.f};
    UPROPERTY(EditAnywhere)
    int32 laser_damage{40};

    UPROPERTY(EditAnywhere)
    FDrawDebugConfig draw_config;
};

USTRUCT()
struct FTestFlySeekDestroyTargetState {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere)
    TWeakObjectPtr<AActor> target{nullptr};
};

UCLASS()
class ATestFlySeekDestroy : public ATestFlyBase {
  public:
    GENERATED_BODY()

    ATestFlySeekDestroy();

    void Tick(float dt) override;
  protected:
    void OnConstruction(FTransform const& t) override;
    void BeginPlay() override;

    // Movement
    void move_to_location(float dt, FVector const& location);
    auto within_radius(FVector const& point, float const r) const -> bool;

    // State
    void set_state(ETestFlySeekDestroyState new_state);
    void transition_to_state();
    template <typename T>
    void assign_state_data(T const& state_data);

    // Search
    void handle_search(float dt);
    void set_new_search_destination();
    auto scan_for_target() -> bool;

    // Chase
    void handle_chase(float dt);

    // Attack
    void handle_attack(float dt);
    void fire_laser();

    // Debugging
    void draw_debug_shapes();

    // Movement
    UPROPERTY(EditAnywhere, Category = "Fly|Movement")
    float turn_speed_deg_per_s{60.f};
    UPROPERTY(VisibleAnywhere, Category = "Fly|Movement")
    float speed{0.f};

    // Vision
    UPROPERTY(EditAnywhere, Category = "Fly|Vision")
    FVisionConfig vision{5000.f, 25.f};

    // State
    UPROPERTY(EditAnywhere, Category = "Fly|State")
    ETestFlySeekDestroyState state{ETestFlySeekDestroyState::searching};
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroySearchState search_state;
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyChaseState chase_state;
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyAttackState attack_state;
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyTargetState target_state;

    // Debugging
    UPROPERTY(EditAnywhere, Category = "Fly|Debug")
    bool show_debug_shapes{false};
    UPROPERTY(EditAnywhere, Category = "Fly|Debug")
    FActorLoggingConfig log_config{1.f};
};
