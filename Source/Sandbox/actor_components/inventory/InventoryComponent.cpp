#include "Sandbox/actor_components/inventory/InventoryComponent.h"

UInventoryComponent::UInventoryComponent() {}

bool UInventoryComponent::add_item(TScriptInterface<IInventoryItem> item) {
    slots.Add({item});

    return true;
}
