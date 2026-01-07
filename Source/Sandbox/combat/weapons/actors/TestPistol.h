#pragma once

#include "CoreMinimal.h"

#include "Sandbox/combat/projectiles/data/generated/BulletTypeIndex.h"
#include "Sandbox/combat/weapons/actors/MassBulletWeapon.h"
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
    : public AMassBulletWeapon
    , public IDescribable {
    GENERATED_BODY()
  public:
    static constexpr StaticTCharString tag{"ATestPistol"};

    ATestPistol();

    // IWeaponInterface
    virtual bool can_fire() const override;
    virtual void start_firing() override;
    virtual void sustain_firing(float delta_time) override { return; }
    virtual void stop_firing() override { return; }

    virtual bool can_reload() const override;

    virtual UStaticMesh* get_display_mesh() const override;
    virtual FString const& get_name() const {
        static FString const default_name{TEXT("ATestPistol")};
        return default_name;
    };

    // IDescribable
    virtual FText get_description() const override {
        static auto const desc{FText::FromName(TEXT("Test Pistol"))};
        return desc;
    }
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    float bullet_speed{5000.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    UStaticMeshComponent* gun_mesh_component{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    UArrowComponent* fire_point_arrow{nullptr};
};
