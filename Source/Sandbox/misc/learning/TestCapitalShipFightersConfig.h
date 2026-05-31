#pragma once

#include "Sandbox/utilities/DrawDebugConfig.h"

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "TestCapitalShipFightersConfig.generated.h"

class UStaticMesh;

UCLASS(BlueprintType)
class UTestCapitalShipFightersConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    UTestCapitalShipFightersConfig() = default;

    // Visuals
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UStaticMesh> mesh{nullptr};

    // Movement
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float speed{2000.f};

    // Combat
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 health{50};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float fire_cooldown{0.33f};

    // Debugging
    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;
};
