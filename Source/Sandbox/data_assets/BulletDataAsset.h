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
        FPrimaryAssetType const asset_type{TEXT("Bullet")};
        FName const asset_name{id.IsNone() ? GetFName() : id};
        FPrimaryAssetId const full_id{asset_type, asset_name};

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
        UE_LOG(LogTemp,
               Verbose,
               TEXT("Asset name is %s, %s"),
               *asset_type.GetName().ToString(),
               *asset_name.ToString());
#endif

        return full_id;
    }
};
