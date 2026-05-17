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

UENUM()
enum class ETestFlySeekDestroyEvadeState : uint8 {
    searching,
    chasing,
    attacking,
    evading,
};

USTRUCT()
struct FTestFlySeekDestroyEvadeMovementConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Fly|Movement")
    float turn_speed_deg_per_s{60.f};
    UPROPERTY(VisibleAnywhere, Category = "Fly|Movement")
    float speed{0.f};
};

USTRUCT()
struct FTestFlySeekDestroyEvadeSearchStateConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeMovementConfig movement;
};

USTRUCT()
struct FTestFlySeekDestroyEvadeChaseStateConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeMovementConfig movement;
};

USTRUCT()
struct FTestFlySeekDestroyEvadeAttackStateConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeMovementConfig movement;
};

USTRUCT()
struct FTestFlySeekDestroyEvadeEvadeStateConfig {
    GENERATED_BODY()

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
};

UCLASS()
class ATestFlySeekDestroyEvade : public ATestFlyBase {
  public:
    GENERATED_BODY()

    ATestFlySeekDestroyEvade();

    void Tick(float dt) override;

    void set_config(FTestFlySeekDestroyEvadeConfig const& new_config);
  protected:
    void OnConstruction(FTransform const& t) override;
    void BeginPlay() override;

    void set_state(ETestFlySeekDestroyEvadeState new_state);
    void transition_to_state();

    void handle_search(float dt);
    void handle_chase(float dt);
    void handle_attack(float dt);
    void handle_evade(float dt);

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
