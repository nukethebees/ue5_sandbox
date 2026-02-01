#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/combat/weapons/enums/AmmoType.h"
#include "Sandbox/interaction/interfaces/Describable.h"
#include "Sandbox/interaction/interfaces/Interactable.h"
#include "Sandbox/inventory/data/Dimensions.h"
#include "Sandbox/inventory/enums/ItemType.h"
#include "Sandbox/inventory/interfaces/InventoryItem.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "AmmoItem.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UTexture2D;

UCLASS()
class SANDBOX_API AAmmoItem
    : public AActor
    , public IDescribable
    , public IInventoryItem
    , public IInteractable
    , public ml::LogMsgMixin<"AAmmoItem", LogSandboxWeapon> {
    GENERATED_BODY()
  public:
    AAmmoItem();

    // IDescribable
    virtual FText get_description() const override {
        return FText::FromString(display_name);
    }

    // IInventoryItem
    UFUNCTION()
    virtual FDimensions get_size() const override { return FDimensions{1, 1}; };
    UFUNCTION()
    virtual FString const& get_name() const {
        return display_name;
    };
    virtual UTexture2D* get_display_image() const override { return display_image; }
    virtual EItemType get_item_type() const override { return EItemType::Ammo; }
    virtual FStackSize get_quantity() const { return FStackSize{quantity}; }

    // IInteractable
    virtual void on_interacted(AActor& instigator) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
    EAmmoType ammo_type{EAmmoType::Bullets};
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    UBoxComponent* collision_box{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    UStaticMeshComponent* mesh_component;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UTexture2D* display_image{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
    int32 quantity{50};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
    FString display_name{"Ammo"};
};
