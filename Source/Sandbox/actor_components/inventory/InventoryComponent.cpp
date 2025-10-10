#include "Sandbox/actor_components/inventory/InventoryComponent.h"

#include "Sandbox/actors/weapons/WeaponBase.h"

UInventoryComponent::UInventoryComponent() {}

bool UInventoryComponent::add_item(TScriptInterface<IInventoryItem> item) {
    if (!item_fits(*item)) {
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

    for (auto& slot : slots) {
        if (slot.item->is_weapon()) {
            return Cast<AWeaponBase>(slot.item.GetObject());
        }
    }

    return nullptr;
}
