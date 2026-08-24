#pragma once

#include "CoreMinimal.h"
#include "Containers/EnumAsByte.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"

#include "TestLasersConfig.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UNiagaraSystem;

UCLASS()
class UTestLasersConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    auto is_ready() const noexcept -> bool;

    // Visuals
    UPROPERTY(EditAnywhere)
    TObjectPtr<UStaticMesh> mesh;

    UPROPERTY(EditAnywhere)
    TObjectPtr<UMaterialInterface> material;

    UPROPERTY(EditAnywhere)
    float min_cull_distance{0.0f};
    UPROPERTY(EditAnywhere)
    float max_cull_distance{50000.0f};

    UPROPERTY(EditAnywhere)
    TObjectPtr<UNiagaraSystem> hit_effect{nullptr};

    // Collision
    UPROPERTY(EditAnywhere)
    TEnumAsByte<ECollisionChannel> collision_channel{ECC_Visibility};
};
