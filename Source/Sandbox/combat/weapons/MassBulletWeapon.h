#pragma once

#include "CoreMinimal.h"

#include "Sandbox/combat/bullets/BulletTypeIndex.h"
#include "Sandbox/combat/weapons/WeaponBase.h"
#include "Sandbox/interaction/interfaces/Describable.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/string_literal_wrapper.h"

#include "MassBulletWeapon.generated.h"

class UStaticMeshComponent;
class UArrowComponent;
class UBulletDataAsset;

UCLASS(Abstract)
class SANDBOX_API AMassBulletWeapon : public AWeaponBase {
    GENERATED_BODY()
  public:
    static constexpr StaticTCharString tag{"AMassBulletWeapon"};
  protected:
    virtual void BeginPlay() override;
#if WITH_EDITOR
    virtual void OnConstruction(FTransform const& Transform) override;
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    TObjectPtr<UBulletDataAsset> bullet_data{nullptr};

    TOptional<FBulletTypeIndex> cached_bullet_type_index;
};
