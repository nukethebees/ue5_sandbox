#include "Sandbox/data/trigger/WeaponPickupPayload.h"

#include "Sandbox/actor_components/weapons/PawnWeaponComponent.h"
#include "Sandbox/actors/weapons/WeaponBase.h"

#include "Sandbox/macros/null_checks.hpp"

FTriggerResult FWeaponPickupPayload::trigger(FTriggerContext context) {
    RETURN_VALUE_IF_FALSE(weapon, FTriggerResult{});
    RETURN_VALUE_IF_FALSE(context.source.instigator, FTriggerResult{});

    INIT_PTR_OR_RETURN_VALUE(weapon_component,
                             context.source.instigator->GetComponentByClass<UPawnWeaponComponent>(),
                             FTriggerResult{});

    if (weapon_component->pickup_new_weapon(*weapon)) {
        weapon->set_pickup_collision(false);
    }

    return FTriggerResult{};
}
bool FWeaponPickupPayload::tick(float delta_time) {
    return false;
}
