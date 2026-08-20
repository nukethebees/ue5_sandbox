#include "ShooterGame/combat/weapons/WeaponPickupPayload.h"

#include "ShooterGame/combat/pawn_weapon_component/PawnWeaponComponent.h"
#include "ShooterGame/combat/weapons/WeaponBase.h"
#include "ShooterGame/inventory/InventoryComponent.h"

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

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
