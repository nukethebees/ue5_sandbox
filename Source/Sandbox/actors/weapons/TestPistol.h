#pragma once

#include "CoreMinimal.h"

#include "Sandbox/actors/weapons/WeaponBase.h"

#include "TestPistol.generated.h"

class UStaticMeshComponent;
class UArrowComponent;

UCLASS()
class SANDBOX_API ATestPistol : public AWeaponBase {
    GENERATED_BODY()
  public:
    ATestPistol();

    virtual void start_firing() override;
    virtual void sustain_firing(float DeltaTime) override;
    virtual void stop_firing() override;

    virtual void reload() override;
    virtual bool can_reload() const override;

    virtual bool can_fire() const override;

    virtual EAmmoType get_ammo_type() const override;
    virtual FAmmo get_current_ammo() const override;
    virtual FAmmo get_max_ammo() const override;
  protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    FAmmo ammo{0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    UStaticMeshComponent* gun_mesh{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    UArrowComponent* fire_point_arrow{nullptr};
};
