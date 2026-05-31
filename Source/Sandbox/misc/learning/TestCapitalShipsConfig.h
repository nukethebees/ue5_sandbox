#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "TestCapitalShipsConfig.generated.h"

class UStaticMesh;

class AShipLaser;

UCLASS(BlueprintType)
class UTestCapitalShipsConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    UTestCapitalShipsConfig() = default;

    // Visuals
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 fighter_spawn_slots{6};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FTransform> fighter_spawn_slots_relative_transforms;
};
