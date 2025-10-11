#include "Sandbox/actor_components/inventory/InventoryComponent.h"

#include "Algo/RandomShuffle.h"

#include "Sandbox/actors/weapons/WeaponBase.h"

UInventoryComponent::UInventoryComponent() {}

bool UInventoryComponent::add_item(TScriptInterface<IInventoryItem> item) {
    if (!item_fits(*item)) {
        log_error(TEXT("Failed to add item to inventory."));
        return false;
    }

    slots.Add({item});

    return true;
}
bool UInventoryComponent::item_fits(IInventoryItem const& item) const {
    return true;
}
AWeaponBase* UInventoryComponent::get_random_weapon() {
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
