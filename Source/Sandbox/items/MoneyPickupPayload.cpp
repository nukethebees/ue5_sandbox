#include "Sandbox/items/MoneyPickupPayload.h"

#include "GameFramework/Actor.h"

#include "Sandbox/inventory/InventoryComponent.h"

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
