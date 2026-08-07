#pragma once

#include <Sandbox/utilities/DrawDebugConfig.h>

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "TestStaticTurretsConfig.generated.h"

class UStaticMesh;
class UNiagaraSystem;

class UTestTeamVisualData;

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

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};

    // Misc
    UPROPERTY(EditAnywhere, Category = "Awareness")
    float detection_radius{3000.f};

    UPROPERTY(EditAnywhere, Category = "Combat")
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

    UPROPERTY(EditAnywhere, Category = "Health")
    int32 max_health{20};

    // Proxy settings
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool show_collision{false};

    // Debug
    UPROPERTY(EditAnywhere, Category = "Debug")
    FDrawDebugConfig debug_drawer;

    UPROPERTY(EditAnywhere, Category = "Debug")
    FVector debug_status_text_offset{0.0, 0.0, 500.0};
};
