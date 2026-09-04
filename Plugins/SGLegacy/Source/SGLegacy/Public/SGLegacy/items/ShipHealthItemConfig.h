#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "ShipHealthItemConfig.generated.h"

class UMaterialInterface;

UCLASS(BlueprintType)
class UShipHealthItemConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UMaterialInterface* material{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float health{5.f};
};
