#pragma once

#include <optional>

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Sandbox/inventory/data/InventorySlot.h"
#include "Sandbox/inventory/interfaces/InventoryItem.h"
#include "Sandbox/inventory/Inventory.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "InventoryComponent.generated.h"

class AWeaponBase;

UCLASS()
class SANDBOX_API UInventoryComponent
    : public UActorComponent
    , public ml::LogMsgMixin<"UInventoryComponent", LogSandboxInventory> {
    GENERATED_BODY()
  public:
    UInventoryComponent();

    // Item insertion
    bool add_item(TScriptInterface<IInventoryItem> item);
    bool move_item(FInventorySlot const& slot, FCoord item_offset, FCoord drop_location);
    auto find_free_point(IInventoryItem const& item) const -> std::optional<FCoord>;

    // Item access
    auto get_random_weapon() -> AWeaponBase*;
    auto get_first_weapon() -> AWeaponBase*;
    auto get_last_weapon() -> AWeaponBase*;
    auto get_weapon_after(AWeaponBase const& weapon) -> AWeaponBase*;
    auto get_weapon_before(AWeaponBase const& weapon) -> AWeaponBase*;

    // Money
    void add_money(int32 new_money) { money += new_money; }
    auto get_money() const { return money; }

    // Grid dimensions
    auto get_dimensions() const { return dimensions; }
    auto get_n_rows() const { return dimensions.y(); }
    auto get_n_columns() const { return dimensions.x(); }
    auto get_n_slots() const { return dimensions.area(); }
    auto get_width() const { return dimensions.x(); }
    auto get_height() const { return dimensions.y(); }
    auto get_aspect_ratio() const {
        return static_cast<float>(dimensions.x()) / static_cast<float>(dimensions.y());
    }

    // Grid querying
    bool is_free(FCoord coord,
                 FDimensions item_dimensions,
                 FInventorySlot const* to_ignore = nullptr) const;
    // If origin maps to an item's origin, return it
    auto get_item_at_origin(FCoord coord) -> FInventorySlot*;
    auto get_item(AWeaponBase const& weapon) -> FInventorySlot*;

    auto get_slots_view() const { return TArrayView<FInventorySlot const>(slots); }
  protected:
    template <auto next_index_fn>
    auto get_weapon_adjacent(AWeaponBase const& cur_weapon) -> AWeaponBase*;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TArray<FInventorySlot> slots{};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    FDimensions dimensions{100, 100};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    int32 money{0};
};
