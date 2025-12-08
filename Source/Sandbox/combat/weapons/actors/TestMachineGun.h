#pragma once

#include "CoreMinimal.h"

#include "Sandbox/combat/weapons/actors/WeaponBase.h"
#include "Sandbox/interaction/interfaces/Describable.h"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "TestMachineGun.generated.h"

class UStaticMeshComponent;
class UArrowComponent;
class UBulletDataAsset;

UCLASS()
class SANDBOX_API ATestMachineGun
    : public AWeaponBase
    , public IDescribable {
    GENERATED_BODY()
  public:
    ATestMachineGun();

    virtual void start_firing() override;
    virtual void sustain_firing(float delta_time) override;
    virtual void stop_firing() override;
    virtual bool can_fire() const override;

    virtual FAmmoReloadResult reload(FAmmoData const& offered) override;
    virtual bool can_reload() const override;

    int32 get_ammo_needed() const;

    virtual UStaticMesh* get_display_mesh() const override;
    virtual FString const& get_name() const {
        static FString const default_name{TEXT("ATestMachineGun")};
        return default_name;
    };

    // IDescribable
    virtual FText const& get_description() const override {
        static auto const desc{FText::FromName(TEXT("Test Machine Gun"))};
        return desc;
    }
  protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    float fire_rate{10.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    float bullet_speed{5000.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    TObjectPtr<UBulletDataAsset> bullet_data{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    UStaticMeshComponent* gun_mesh_component{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    UArrowComponent* fire_point_arrow{nullptr};
  private:
    void fire_single_bullet(float travel_time);
    FTransform get_spawn_transform(float travel_time) const;

    bool is_firing{false};
    float time_since_last_shot{0.0f};
};
