#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "ShipLaserConfig.generated.h"

class UMaterialInterface;

UCLASS(BlueprintType)
class UShipLaserConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UMaterialInterface* material{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float speed{10000.f};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 damage{10};
};
