#pragma once

#include "CoreMinimal.h"

#include "Sandbox/combat/weapons/WeaponBase.h"
#include "Sandbox/interaction/Describable.h"

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

    virtual bool can_fire() const override;
    virtual void start_firing() override;
    virtual void sustain_firing(float delta_time) override { return; }
    virtual void stop_firing() override { return; }
    virtual bool can_reload() const override;

    // IDescribable
    virtual FText get_description() const override {
        static auto const desc{FText::FromName(TEXT("Rocket Launcher"))};
        return desc;
    }

    // IInventoryItem
    virtual FString const& get_name() const override {
        static FString const default_name{TEXT("Rocket Launcher")};
        return default_name;
    };
  protected:
    UPROPERTY(EditAnywhere, Category = "Rocket")
    UStaticMeshComponent* mesh;
    UPROPERTY(EditAnywhere, Category = "Rocket")
    TSubclassOf<ARocket> rocket_class;
    UPROPERTY(EditAnywhere, Category = "Rocket")
    float rocket_speed{500.f};
    UPROPERTY(EditAnywhere, Category = "Rocket")
    float explosion_radius{100.f};
};
