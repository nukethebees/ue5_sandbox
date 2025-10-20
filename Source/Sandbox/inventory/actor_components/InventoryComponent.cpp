#include "Sandbox/inventory/actor_components/InventoryComponent.h"

#include "Algo/RandomShuffle.h"

#include "Sandbox/combat/weapons/actors/WeaponBase.h"

UInventoryComponent::UInventoryComponent() {}

bool UInventoryComponent::add_item(TScriptInterface<IInventoryItem> item) {
    constexpr auto logger{NestedLogger<"add_item">()};
    auto const free_point{find_free_point(*item)};

    if (!free_point) {
        log_warning(TEXT("Couldn't find a free spot in the inventory."));
        return false;
    }

    FStackSize const stack_size{1};
    FInventorySlot slot{item, stack_size, item->get_size(), *free_point};

    logger.log_display(TEXT("Adding %s to %s"), *item->get_name(), *free_point->to_string());

    slots.Add(slot);

    return true;
}
auto UInventoryComponent::find_free_point(IInventoryItem const& item) const
    -> std::optional<FCoord> {
    auto const size{item.get_size()};
    auto const n_rows{get_n_rows()};
    auto const n_cols{get_n_columns()};

    for (int32 row{0}; row < n_rows; row++) {
        for (int32 col{0}; col < n_cols; col++) {
            FCoord coord{col, row};
            if (is_free(coord, size)) {
                return coord;
            }
        }
    }

    return {};
}
auto UInventoryComponent::get_random_weapon() -> AWeaponBase* {
    log_display(TEXT("Getting random weapon."));

    TArray<int32> indexes;
    auto const n{slots.Num()};
    indexes.Reserve(n);
    for (int32 i{0}; i < n; ++i) {
        indexes.Add(i);
    }
    Algo::RandomShuffle(indexes);

    for (int32 i : indexes) {
        auto& slot{slots[i]};
        if (slot.item->is_weapon()) {
            return Cast<AWeaponBase>(slot.item.GetObject());
        }
    }

    return nullptr;
}
bool UInventoryComponent::is_free(FCoord coord, FDimensions item_dimensions) const {
    constexpr auto logger{NestedLogger<"is_free">()};

    auto const item_width{item_dimensions.x()};
    auto const item_height{item_dimensions.y()};

    auto const ax0{coord.x()};
    auto const ax1{ax0 + item_width};

    auto const ay0{coord.y()};
    auto const ay1{ay0 + item_height};

    if (ax0 < 0) {
        logger.log_verbose(TEXT("ax0 is negative. (%d)"), ax0);
        return false;
    }
    if (ay0 < 0) {
        logger.log_verbose(TEXT("ay0 is negative. (%d)"), ay0);
        return false;
    }

    auto const inventory_width{get_width()};
    auto const inventory_height{get_height()};

    if (ax1 > inventory_width) {
        logger.log_verbose(TEXT("ax1 beyond inventory width. (%d vs %d)"), ax1, inventory_width);
        return false;
    }
    if (ay1 > inventory_height) {
        logger.log_verbose(TEXT("ay1 beyond inventory height. (%d vs %d)"), ay1, inventory_height);
        return false;
    }

    for (auto const& slot : slots) {
        // AABB collision
        // Check if A's left edge is to the left of B's right edge
        // and if A's right edge is to the right of B's left edge
        //
        // Check if A's top is above B's bottom and A's bottom is below B's top
        auto const bx{slot.origin.x()};
        auto const by{slot.origin.y()};

        auto const bw{slot.width()};
        auto const bh{slot.height()};

        auto const intersects{
            (ax0 < (bx + bw))    // A's left < B's right
            && (ax1 > bx)        // A's right > B's left
            && (ay0 < (by + bh)) // A's bottom < B's top
            && (ay1 > by)        // A's top > B's bottom
        };
        if (intersects) {
            logger.log_verbose(TEXT("Intersected with slot item %s"), *slot.name());
            return false;
        }
    }

    return true;
}
