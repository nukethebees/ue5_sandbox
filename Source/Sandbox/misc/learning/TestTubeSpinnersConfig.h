#pragma once

#include "Sandbox/utilities/DrawDebugConfig.h"

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "TestTubeSpinnersConfig.generated.h"

class UStaticMesh;

UCLASS(BlueprintType)
class UTestTubeSpinnersConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    UTestTubeSpinnersConfig() = default;

    // Visuals
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UStaticMesh> mesh{nullptr};

    // Fire points
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FTransform> fire_point_offsets{};

    // Movement
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float yaw_rotation_speed_degrees{66.f};

    // Combat
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 laser_damage{2};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float attack_cooldown{0.05f};

    // Debugging
    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;
};
