#pragma once

#include "CoreMinimal.h"

#include "Sandbox/combat/weapons/actors/WeaponBase.h"
#include "Sandbox/interaction/interfaces/Describable.h"

#include "RocketLauncher.generated.h"

class UStaticMeshComponent;

class ARocket;

UCLASS()
class SANDBOX_API ARocketLauncher
    : public AWeaponBase
    , public IDescribable {
    GENERATED_BODY()
  public:
    ARocketLauncher();

    // IDescribable
    virtual FText get_description() const override {
        static auto const desc{FText::FromName(TEXT("Rocket Launcher"))};
        return desc;
    }
  protected:
    UPROPERTY(EditAnywhere, Category = "Rocket")
    UStaticMeshComponent* mesh;
    UPROPERTY(EditAnywhere, Category = "Rocket")
    TSubclassOf<ARocket> rocket_class;
};
