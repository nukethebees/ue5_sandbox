#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "PawnWeaponComponent.generated.h"

class UWeaponComponent;
class AWeaponBase;

UCLASS()
class SANDBOX_API UPawnWeaponComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UPawnWeaponComponent() = default;

    bool can_fire() const;
    void start_firing();
    void sustain_firing(float delta_time);
    void stop_firing();

    void reload();
    bool can_reload() const;
  protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapons")
    AWeaponBase* active_weapon{nullptr};
};
