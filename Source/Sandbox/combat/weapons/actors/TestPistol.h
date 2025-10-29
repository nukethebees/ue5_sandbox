#pragma once

#include "CoreMinimal.h"

#include "Sandbox/combat/projectiles/data/generated/BulletTypeIndex.h"
#include "Sandbox/combat/weapons/actors/WeaponBase.h"
#include "Sandbox/interaction/interfaces/Describable.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/string_literal_wrapper.h"

#include "TestPistol.generated.h"

class UStaticMeshComponent;
class UArrowComponent;
class UBulletDataAsset;

UCLASS()
class SANDBOX_API ATestPistol
    : public AWeaponBase
    , public IDescribable {
    GENERATED_BODY()
  public:
    static constexpr StaticTCharString tag{"ATestPistol"};

    ATestPistol();

    virtual FDimensions get_size() const override;

    // IWeaponInterface
    virtual bool can_fire() const override;
    virtual void start_firing() override;
    virtual void sustain_firing(float delta_time) override { return; }
    virtual void stop_firing() override { return; }

    virtual FAmmoReloadResult reload(FAmmoData const& offered) override;
    virtual bool can_reload() const override;

    virtual EAmmoType get_ammo_type() const override { return EAmmoType::Bullets; }
    virtual FAmmoData get_current_ammo() const override;
    virtual FAmmoData get_max_ammo() const override;

    virtual UStaticMesh* get_display_mesh() const override;
    virtual FString const& get_name() const {
        static FString const default_name{TEXT("ATestPistol")};
        return default_name;
    };

    // IInventoryItem
    int32 get_ammo_needed() const;

    // IDescribable
    virtual FText const& get_description() const override {
        static auto const desc{FText::FromName(TEXT("Test Pistol"))};
        return desc;
    }
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
