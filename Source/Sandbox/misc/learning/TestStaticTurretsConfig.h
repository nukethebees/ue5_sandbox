#pragma once

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
};
