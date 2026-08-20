#pragma once

#include "CoreMinimal.h"

#include "ShooterGame/combat/bullets/BulletTypeIndex.h"
#include "ShooterGame/combat/weapons/WeaponBase.h"
#include "ShooterGame/interaction/Describable.h"
#include "ShooterGame/logging/ShooterGameLogCategories.h"
#include "SandboxGameShared/utilities/string_literal_wrapper.h"

#include "MassBulletWeapon.generated.h"

class UStaticMeshComponent;
class UArrowComponent;
class UBulletDataAsset;

UCLASS(Abstract)
class SHOOTERGAME_API AMassBulletWeapon : public AWeaponBase {
    GENERATED_BODY()
  public:
    static constexpr StaticTCharString tag{"AMassBulletWeapon"};
  protected:
    void BeginPlay() override;
#if WITH_EDITOR
    virtual void OnConstruction(FTransform const& Transform) override;
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    TObjectPtr<UBulletDataAsset> bullet_data{nullptr};

    TOptional<FBulletTypeIndex> cached_bullet_type_index;
};
