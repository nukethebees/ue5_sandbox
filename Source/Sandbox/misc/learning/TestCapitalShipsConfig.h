#pragma once

#include "Sandbox/utilities/DrawDebugConfig.h"

#include "SandboxCore/collision_settings.h"

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"

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

    // Collision
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector collision_box_extent{FVector::ZeroVector};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FCollisionSettings collision_settings;

    // Fighter spawning
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float spawn_delay{5.f};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 fighter_spawn_slots{6};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FTransform> fighter_spawn_slots_relative_transforms;

    // Health
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 max_health{5000};

    // Debugging
    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;
  private:
    void synchronise_data();
};
