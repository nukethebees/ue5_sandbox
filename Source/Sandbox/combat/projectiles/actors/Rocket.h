#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/combat/weapons/actors/AmmoItem.h"
#include "Sandbox/interaction/interfaces/Describable.h"
#include "Sandbox/interaction/interfaces/Interactable.h"
#include "Sandbox/inventory/enums/ItemType.h"
#include "Sandbox/inventory/interfaces/InventoryItem.h"

#include "Rocket.generated.h"

class UStaticMeshComponent;
class UTexture2D;
class UProjectileMovementComponent;

UCLASS()
class SANDBOX_API ARocket : public AAmmoItem {
    GENERATED_BODY()
  public:
    ARocket();

    void Tick(float dt) override;

    // IDescribable
    virtual FText get_description() const override {
        static auto const desc{FText::FromName(TEXT("Rocket"))};
        return desc;
    }

    // IInventoryItem
    virtual FDimensions get_size() const override { return FDimensions{1, 1}; };
    virtual FString const& get_name() const {
        static FString const default_name{TEXT("Rocket")};
        return default_name;
    };
    virtual EItemType get_item_type() const override { return EItemType::Ammo; }
  protected:
    void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bullet")
    UProjectileMovementComponent* projectile_movement{nullptr};
};
