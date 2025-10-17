#pragma once

#include <optional>

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
    auto find_free_point(IInventoryItem const& item) const -> std::optional<FCoord>;
    auto get_random_weapon() -> AWeaponBase*;
    auto get_n_rows() const { return dimensions.y(); }
    auto get_n_columns() const { return dimensions.x(); }
    bool is_free(FCoord coord, FDimensions item_dimensions) const;

    auto get_width() const { return dimensions.x(); }
    auto get_height() const { return dimensions.y(); }
  protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TArray<FInventorySlot> slots{};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    FDimensions dimensions{100, 100};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    int32 money{0};
};
