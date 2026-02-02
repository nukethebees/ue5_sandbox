#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "Sandbox/health/HealthChange.h"

#include "BulletDataAsset.generated.h"

class UNiagaraSystem;
class UStaticMesh;
class UNiagaraDataChannelAsset;
class UMaterial;

UCLASS()
class SANDBOX_API UBulletDataAsset : public UPrimaryDataAsset {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
    FName id{NAME_None};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet")
    TObjectPtr<UNiagaraSystem> impact_effect{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet")
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet")
    TObjectPtr<UMaterial> material{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet")
    TObjectPtr<UNiagaraDataChannelAsset> ndc_asset{nullptr};

    virtual FPrimaryAssetId GetPrimaryAssetId() const override {
        static FPrimaryAssetType const asset_type{TEXT("Bullet")};
        FName const asset_name{id.IsNone() ? GetFName() : id};
        FPrimaryAssetId const full_id{asset_type, asset_name};

        return full_id;
    }
};
