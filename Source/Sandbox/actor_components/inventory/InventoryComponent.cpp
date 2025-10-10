#include "Sandbox/actor_components/inventory/InventoryComponent.h"

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
