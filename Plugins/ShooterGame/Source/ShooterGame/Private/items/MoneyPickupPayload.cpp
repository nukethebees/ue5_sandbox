#include "ShooterGame/items/MoneyPickupPayload.h"

#include "GameFramework/Actor.h"

#include "ShooterGame/inventory/InventoryComponent.h"

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

FTriggerResult FMoneyPickupPayload::trigger(FTriggerContext context) {
    auto* actor{context.source.instigator};
    RETURN_VALUE_IF_NULLPTR(actor, {});

    if (auto* inv{actor->FindComponentByClass<UInventoryComponent>()}) {
        inv->add_money(this->money);
        context.triggered_actor.Destroy();
    }

    return {};
}
