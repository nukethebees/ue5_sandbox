#pragma once

#include "CoreMinimal.h"

#include "Sandbox/actors/weapons/WeaponBase.h"
#include "Sandbox/generated/strong_typedefs/BulletTypeIndex.h"
#include "Sandbox/SandboxLogCategories.h"

#include "TestPistol.generated.h"

class UStaticMeshComponent;
class UArrowComponent;
class UBulletDataAsset;

UCLASS()
class SANDBOX_API ATestPistol : public AWeaponBase {
    GENERATED_BODY()
  public:
    ATestPistol();

    virtual void start_firing() override;
    virtual void sustain_firing(float delta_time) override { return; }
    virtual void stop_firing() override { return; }
    virtual bool can_fire() const override;

    virtual FAmmoReloadResult reload(FAmmoData const& offered) override;
    virtual bool can_reload() const override;

    virtual EAmmoType get_ammo_type() const override { return EAmmoType::Bullets; }
    virtual FAmmoData get_current_ammo() const override;
    virtual FAmmoData get_max_ammo() const override;
    int32 get_ammo_needed() const;

    virtual UStaticMesh* get_display_mesh() const override;
    virtual FString const& get_name() const {
        static FString const default_name{TEXT("ATestPistol")};
        return default_name;
    };
  protected:
    virtual void BeginPlay() override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    int32 max_ammo{10};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    int32 ammo{0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    float bullet_speed{5000.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    TObjectPtr<UBulletDataAsset> bullet_data{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    UStaticMeshComponent* gun_mesh_component{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    UArrowComponent* fire_point_arrow{nullptr};

    TOptional<FBulletTypeIndex> cached_bullet_type_index;
};
