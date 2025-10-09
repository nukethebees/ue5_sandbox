#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "PawnWeaponComponent.generated.h"

class UWeaponComponent;

UCLASS()
class SANDBOX_API UPawnWeaponComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UPawnWeaponComponent();
  protected:
    virtual void OnRegister() override;
    virtual void OnUnregister() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapons")
    UWeaponComponent* weapon_component{nullptr};
};
