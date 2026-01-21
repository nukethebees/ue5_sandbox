#pragma once

#include "CoreMinimal.h"

#include "Sandbox/combat/weapons/actors/WeaponBase.h"

#include "RocketLauncher.generated.h"

class UStaticMeshComponent;

class ARocket;

UCLASS()
class SANDBOX_API ARocketLauncher : public AWeaponBase {
    GENERATED_BODY()
  public:
    ARocketLauncher();
  protected:
    UPROPERTY(EditAnywhere, Category = "Rocket")
    UStaticMeshComponent* mesh;
    UPROPERTY(EditAnywhere, Category = "Rocket")
    TSubclassOf<ARocket> rocket_class;
};
