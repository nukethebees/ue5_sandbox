#include "Sandbox/combat/weapons/WeaponPickupPayload.h"

#include "Sandbox/combat/pawn_weapon_component/PawnWeaponComponent.h"
#include "Sandbox/combat/weapons/WeaponBase.h"
#include "Sandbox/inventory/actor_components/InventoryComponent.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

FTriggerResult FWeaponPickupPayload::trigger(FTriggerContext context) {
    RETURN_VALUE_IF_FALSE(weapon, FTriggerResult{});
    RETURN_VALUE_IF_FALSE(context.source.instigator, FTriggerResult{});

    if (UInventoryComponent::add_item(*context.source.instigator,
                                      TScriptInterface<AWeaponBase>(weapon))) {
        weapon->set_pickup_collision(false);
    }

    return FTriggerResult{};
}
bool FWeaponPickupPayload::tick(float delta_time) {
    return false;
}
