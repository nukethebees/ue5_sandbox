#pragma once

#include "CoreMinimal.h"

#include "Sandbox/combat/weapons/enums/AmmoType.h"
#include "Sandbox/interaction/interfaces/Describable.h"
#include "Sandbox/inventory/data/Dimensions.h"
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
    , public ml::LogMsgMixin<"AAmmoItem", LogSandboxWeapon> {
    GENERATED_BODY()
  public:
    AAmmoItem();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
    EAmmoType ammo_type{EAmmoType::Bullets};

    // IDescribable
    virtual FText const& get_description() const override {
        static auto const desc{FText::FromName(TEXT("Ammo"))};
        return desc;
    }

    // IInventoryItem
    UFUNCTION()
    virtual FDimensions get_size() const override { return FDimensions{1, 1}; };
    UFUNCTION()
    virtual FString const& get_name() const {
        static FString const inventory_name{TEXT("Ammo")};
        return inventory_name;
    };
    virtual UTexture2D* get_display_image() const override { return display_image; }
  protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    UBoxComponent* collision_box{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    UStaticMeshComponent* mesh_component;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UTexture2D* display_image{nullptr};
  protected:
    virtual void BeginPlay() override;
};
