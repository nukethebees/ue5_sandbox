#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "Sandbox/enums/AmmoType.h"

#include "WeaponArchetypeDataAsset.generated.h"

class UStaticMesh;

UCLASS()
class SANDBOX_API UWeaponArchetypeDataAsset : public UPrimaryDataAsset {
    GENERATED_BODY()
  public:
    UWeaponArchetypeDataAsset() = default;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName name{};
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UStaticMesh* weapon_mesh{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float firing_rate{0.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float damage{0.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float reload_time{0.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TOptional<float> magazine_size{};

    virtual FPrimaryAssetId GetPrimaryAssetId() const override {
        static FPrimaryAssetType const asset_type{TEXT("Weapon")};
        FName const asset_name{name.IsNone() ? GetFName() : name};
        FPrimaryAssetId const full_id{asset_type, asset_name};

        return full_id;
    }
};
