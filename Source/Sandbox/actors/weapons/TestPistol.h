#pragma once

#include "CoreMinimal.h"

#include "Sandbox/actors/weapons/WeaponBase.h"
#include "Sandbox/SandboxLogCategories.h"

#include "TestPistol.generated.h"

class UStaticMeshComponent;
class UArrowComponent;

UCLASS()
class SANDBOX_API ATestPistol : public AWeaponBase {
    GENERATED_BODY()
  public:
    ATestPistol();

    virtual void start_firing() override;
    virtual void sustain_firing(float delta_time) override { return; }
    virtual void stop_firing() override { return; }
    virtual bool can_fire() const override { return ammo > FAmmo{0}; }

    virtual FAmmoReloadResult reload(FAmmo offered) override {
        if (!can_reload()) {
            return {FAmmo{}, offered};
        }

        auto const needed{get_ammo_needed()};
        auto const taken{FMath::Min(needed, offered)};
        ammo += taken;

        return {taken, offered - taken};
    }
    virtual bool can_reload() const override { return ammo < max_ammo; }

    virtual EAmmoType get_ammo_type() const override { return EAmmoType::Bullets; }
    virtual FAmmo get_current_ammo() const override { return ammo; }
    virtual FAmmo get_max_ammo() const override { return max_ammo; }
    FAmmo get_ammo_needed() const { return max_ammo - ammo; }

    virtual UStaticMesh* get_display_mesh() const override;
    virtual FString const& get_name() const {
        static FString const default_name{TEXT("ATestPistol")};
        return default_name;
    };
  protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    FAmmo max_ammo{10};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    FAmmo ammo{0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    UStaticMeshComponent* gun_mesh_component{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    UArrowComponent* fire_point_arrow{nullptr};
};
