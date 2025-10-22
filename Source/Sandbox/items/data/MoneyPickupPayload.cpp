#include "Sandbox/items/data/MoneyPickupPayload.h"

#include "GameFramework/Actor.h"

#include "Sandbox/inventory/actor_components/InventoryComponent.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

FTriggerResult FMoneyPickupPayload::trigger(FTriggerContext context) {
    auto* actor{context.source.instigator};
    RETURN_VALUE_IF_NULLPTR(actor, {});

    if (auto* inv{actor->FindComponentByClass<UInventoryComponent>()}) {
        inv->add_money(this->money);
        context.triggered_actor.Destroy();
    }

    return {};
}
