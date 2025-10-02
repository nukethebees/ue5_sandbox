#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "BulletDataAsset.generated.h"

class UNiagaraSystem;
class UStaticMesh;

UCLASS()
class SANDBOX_API UBulletDataAsset : public UPrimaryDataAsset {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
    FName id{NAME_None};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet")
    UNiagaraSystem* impact_effect{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet")
    UStaticMesh* mesh{nullptr};

    virtual FPrimaryAssetId GetPrimaryAssetId() const override {
        return FPrimaryAssetId("Bullet", id.IsNone() ? GetFName() : id);
    }
};
