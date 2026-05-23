#pragma once

#include "Sandbox/combat/BurstFire.h"
#include "Sandbox/logging/ActorLoggingConfig.h"
#include "Sandbox/misc/learning/TestMaterialConfig.h"
#include "Sandbox/players/VisionConfig.h"
#include "Sandbox/utilities/DrawDebugConfig.h"
#include "TestFlyBase.h"

#include "CoreMinimal.h"

#include "TestFlySeekDestroyEvade.generated.h"

class UArrowComponent;

class ATestVolume;
class AShipLaser;

UENUM()
enum class ETestFlySeekDestroyEvadeState : uint8 {
    searching,
    chasing,
    attacking,
    evading,
};

USTRUCT()
struct FTestFlySeekDestroyEvadeStateVisualsConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;
    UPROPERTY(EditAnywhere)
    FTestMaterialConfig material{};
};

USTRUCT()
struct FTestFlySeekDestroyEvadeMovementConfig {
    GENERATED_BODY()

    FTestFlySeekDestroyEvadeMovementConfig() noexcept = default;
    FTestFlySeekDestroyEvadeMovementConfig(float turn_speed_deg_per_s, float speed)
        : turn_speed_deg_per_s(turn_speed_deg_per_s)
        , speed(speed) {}

    UPROPERTY(EditAnywhere)
    float turn_speed_deg_per_s{60.f};
    UPROPERTY(VisibleAnywhere)
    float speed{3000.f};
};

USTRUCT()
struct FTestFlySeekDestroyEvadeSearchStateConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FTestFlySeekDestroyEvadeStateVisualsConfig visuals_config{};

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeMovementConfig movement;

    UPROPERTY(EditAnywhere)
    TObjectPtr<ATestVolume> search_volume{nullptr};
    UPROPERTY(EditAnywhere)
    float min_distance_to_new_point{5000.f};
    UPROPERTY(EditAnywhere)
    float acceptance_radius{500.f};
};

USTRUCT()
struct FTestFlySeekDestroyEvadeChaseStateConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FTestFlySeekDestroyEvadeStateVisualsConfig visuals_config{};

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeMovementConfig movement;
};

USTRUCT()
struct FTestFlySeekDestroyEvadeAttackStateConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FTestFlySeekDestroyEvadeStateVisualsConfig visuals_config{};
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeMovementConfig movement;
};

USTRUCT()
struct FTestFlySeekDestroyEvadeEvadeStateConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FTestFlySeekDestroyEvadeStateVisualsConfig visuals_config{};

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeMovementConfig movement;
};

USTRUCT()
struct FTestFlySeekDestroyEvadeConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeSearchStateConfig search;
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeChaseStateConfig chase;
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeAttackStateConfig attack;
    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeEvadeStateConfig evade;
    UPROPERTY(EditAnywhere, Category = "Fly|Vision")
    FVisionConfig vision{5000.f, 25.f};
};

UCLASS()
class ATestFlySeekDestroyEvade : public ATestFlyBase {
  public:
    GENERATED_BODY()

    ATestFlySeekDestroyEvade();

    void Tick(float dt) override;

    // Config
    void set_config(FTestFlySeekDestroyEvadeConfig const& new_config);

    // Movement
    auto within_radius(FVector const& point, float const r) const -> bool;
  protected:
    void OnConstruction(FTransform const& t) override;
    void BeginPlay() override;

    // State
    void set_state(ETestFlySeekDestroyEvadeState new_state);
    void transition_to_state();

    // Search
    void handle_search(float dt);
    void set_new_search_destination();
    auto scan_for_target() -> bool;

    // Chase
    void handle_chase(float dt);

    // Attack
    void handle_attack(float dt);

    // Evade
    void handle_evade(float dt);

    // Movement
    void move_to_location(float dt, FVector const& location);
    void set_movement(FTestFlySeekDestroyEvadeMovementConfig const& new_config);

    UPROPERTY(EditAnywhere, Category = "Fly")
    TObjectPtr<UArrowComponent> fire_point{nullptr};

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeConfig config;

    UPROPERTY(EditAnywhere, Category = "Fly")
    ETestFlySeekDestroyEvadeState state{ETestFlySeekDestroyEvadeState::searching};

    // Movement
    UPROPERTY(EditAnywhere, Category = "Fly|Movement")
    float turn_speed_deg_per_s{60.f};
    UPROPERTY(VisibleAnywhere, Category = "Fly|Movement")
    float speed{0.f};

    UPROPERTY(VisibleAnywhere)
    FVector destination;

    // Target
    UPROPERTY(VisibleAnywhere)
    TWeakObjectPtr<AActor> target{nullptr};
};
