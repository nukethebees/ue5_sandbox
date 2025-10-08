#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "WeaponArchetypeDataAsset.generated.h"

class UStaticMesh;

UCLASS()
class SANDBOX_API UWeaponArchetypeDataAsset : public UPrimaryDataAsset {
    GENERATED_BODY()
  public:
    UStaticMesh* weapon_mesh{nullptr};
    float firing_rate{0.0f};
};
