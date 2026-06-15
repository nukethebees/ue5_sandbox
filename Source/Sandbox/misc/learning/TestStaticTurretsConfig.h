#pragma once

#include <Sandbox/utilities/DrawDebugConfig.h>

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "TestStaticTurretsConfig.generated.h"

class UStaticMesh;

UCLASS(BlueprintType)
class UTestStaticTurretsConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    UTestStaticTurretsConfig() = default;

    // Visuals
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float detection_radius{3000.f};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FTransform fire_point_offset{FTransform::Identity};

    // Combat
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 laser_damage{5};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float attack_cooldown{0.33f};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 max_health{20};

    // Proxy settings
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool show_collision{false};

    // Debugging
    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;

    UPROPERTY(EditAnywhere)
    FVector debug_status_text_offset{0.0, 0.0, 500.0};
};
