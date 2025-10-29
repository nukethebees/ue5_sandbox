#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Sandbox/inventory/data/Dimensions.h"

#include "InventoryItem.generated.h"

class AWeaponBase;
class UTexture2D;

UINTERFACE(MinimalAPI)
class UInventoryItem : public UInterface {
    GENERATED_BODY()
};

class SANDBOX_API IInventoryItem {
    GENERATED_BODY()
  public:
    virtual bool is_weapon() const { return false; }
    virtual AWeaponBase* get_weapon() { return nullptr; }
    virtual FString const& get_name() const = 0;

    // Grid
    virtual FDimensions get_size() const = 0;

    // Display
    virtual UTexture2D* get_display_image() const { return nullptr; }
};
