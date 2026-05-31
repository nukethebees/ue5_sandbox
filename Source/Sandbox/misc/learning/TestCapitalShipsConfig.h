#pragma once

#include "Sandbox/utilities/DrawDebugConfig.h"

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

#if WITH_EDITOR
    void PostEditChangeProperty(FPropertyChangedEvent& event) override;
    void PostLoad() override;
#endif

    // Visuals
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 fighter_spawn_slots{6};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FTransform> fighter_spawn_slots_relative_transforms;

    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;
  private:
    void synchronise_data();
};
