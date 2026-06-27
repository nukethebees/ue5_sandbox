#pragma once

#include <Sandbox/utilities/DrawDebugConfig.h>

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "TestStaticTurretsConfig.generated.h"

class UStaticMesh;
class UNiagaraSystem;

UCLASS(BlueprintType)
class UTestStaticTurretsConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    UTestStaticTurretsConfig() = default;

    // Visuals
    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    UNiagaraSystem* death_effect{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    FVector death_effect_offset{FVector::ZeroVector};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    float death_effect_scale{1.f};

    // Misc
    UPROPERTY(EditAnywhere)
    float detection_radius{3000.f};

    UPROPERTY(EditAnywhere)
    FTransform fire_point_offset{FTransform::Identity};

    // Combat
    UPROPERTY(EditAnywhere, Category = "Combat")
    int32 laser_damage{5};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float laser_speed{10000.f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float laser_max_distance{10000.f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float attack_cooldown{0.33f};

    UPROPERTY(EditAnywhere)
    int32 max_health{20};

    // Proxy settings
    UPROPERTY(EditAnywhere)
    bool show_collision{false};

    // Debugging
    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;

    UPROPERTY(EditAnywhere)
    FVector debug_status_text_offset{0.0, 0.0, 500.0};
};
