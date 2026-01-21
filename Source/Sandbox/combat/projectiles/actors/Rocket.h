#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/interaction/interfaces/Describable.h"
#include "Sandbox/inventory/interfaces/InventoryItem.h"
#include "Sandbox/inventory/enums/ItemType.h"

#include "Rocket.generated.h"

class UStaticMeshComponent;
class UTexture2D;

UCLASS()
class SANDBOX_API ARocket
    : public AActor
    , public IDescribable
    , public IInventoryItem {
    GENERATED_BODY()
  public:
    ARocket();

    // IDescribable
    virtual FText get_description() const override {
        static auto const desc{FText::FromName(TEXT("Rocket"))};
        return desc;
    }

    // IInventoryItem
    UFUNCTION()
    virtual FDimensions get_size() const override { return FDimensions{1, 1}; };
    UFUNCTION()
    virtual FString const& get_name() const {
        static FString const default_name{TEXT("Rocket")};
        return default_name;
    };
    virtual UTexture2D* get_display_image() const override { return nullptr; }
    virtual EItemType get_item_type() const override { return EItemType::Ammo; }
  protected:
    UPROPERTY(EditAnywhere, Category = "Rocket")
    UStaticMeshComponent* mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Rocket")
    float speed{500.f};
};
