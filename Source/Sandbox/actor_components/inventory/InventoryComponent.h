#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Sandbox/data/inventory/InventorySlot.h"
#include "Sandbox/interfaces/inventory/InventoryItem.h"
#include "Sandbox/mixins/LogMsgMixin.hpp"
#include "Sandbox/SandboxLogCategories.h"
#include "Sandbox/typedefs/Inventory.h"

#include "InventoryComponent.generated.h"

class AWeaponBase;

UCLASS()
class SANDBOX_API UInventoryComponent
    : public UActorComponent
    , public ml::LogMsgMixin<"UInventoryComponent", LogSandboxInventory> {
    GENERATED_BODY()
  public:
    UInventoryComponent();

    bool add_item(TScriptInterface<IInventoryItem> item);
    bool item_fits(IInventoryItem const& item) const;
    AWeaponBase* get_random_weapon();
  protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TArray<FInventorySlot> slots{};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FDimensions dimensions{100, 100};
};
