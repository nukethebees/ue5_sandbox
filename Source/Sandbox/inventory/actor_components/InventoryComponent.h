#pragma once

#include <optional>

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Sandbox/combat/weapons/data/AmmoData.h"
#include "Sandbox/inventory/data/Coord.h"
#include "Sandbox/inventory/data/Dimensions.h"
#include "Sandbox/inventory/data/InventoryEntry.h"
#include "Sandbox/inventory/enums/ItemType.h"
#include "Sandbox/inventory/interfaces/InventoryItem.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "InventoryComponent.generated.h"

class AWeaponBase;

DECLARE_DELEGATE_OneParam(FOnWeaponAdded, AWeaponBase& /* new_weapon */);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAmmoAdded, FAmmoData /* total_ammo */);

UCLASS()
class SANDBOX_API UInventoryComponent
    : public UActorComponent
    , public ml::LogMsgMixin<"UInventoryComponent", LogSandboxInventory> {
    GENERATED_BODY()
  public:
    UInventoryComponent();

    // Item insertion
    static bool add_item(AActor& actor, TScriptInterface<IInventoryItem> item);
    bool add_item(TScriptInterface<IInventoryItem> item);
    bool move_item(FInventoryEntry const& slot, FCoord item_offset, FCoord drop_location);
    auto find_free_point(IInventoryItem const& item) const -> std::optional<FCoord>;

    // Item access
    auto get_random_weapon() -> AWeaponBase*;
    auto get_first_weapon() -> AWeaponBase*;
    auto get_last_weapon() -> AWeaponBase*;
    auto get_weapon_after(AWeaponBase const& weapon) -> AWeaponBase*;
    auto get_weapon_before(AWeaponBase const& weapon) -> AWeaponBase*;
    auto request_ammo(FAmmoData ammo_needed) -> FAmmoData;
    auto count_ammo(EAmmoType type) -> FAmmoData;

    // Money
    void add_money(int32 new_money) { money += new_money; }
    auto get_money() const { return money; }

    // Skill points
    int32 get_skill_points() const { return skill_points; }
    int32 take_skill_points(int32 requested) {
        auto const to_take{std::min(requested, skill_points)};
        skill_points -= to_take;
        return to_take;
    }
    void add_skill_points(int32 added) { skill_points += added; }

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
                 FInventoryEntry const* to_ignore = nullptr) const;
    // If origin maps to an item's origin, return it
    auto get_entry_at_origin(FCoord coord) -> FInventoryEntry*;
    auto get_entry(AWeaponBase const& weapon) -> FInventoryEntry*;

    auto get_entries_view() const { return TArrayView<FInventoryEntry const>(item_entries); }

    FOnWeaponAdded on_weapon_added;
    FOnAmmoAdded on_ammo_added;
  protected:
    template <auto next_index_fn>
    auto get_weapon_adjacent(AWeaponBase const& cur_weapon) -> AWeaponBase*;
    void remove_empty_entries();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TArray<FInventoryEntry> item_entries{};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    FDimensions dimensions{100, 100};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    int32 money{0};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    int32 skill_points{0};
};
