#pragma once

#include "CoreMinimal.h"

#include "Sandbox/interaction/StrongIds.h"
#include "Sandbox/interaction/triggering/data/TriggerableId.h"
#include "Sandbox/interaction/triggering/data/TriggerContext.h"
#include "Sandbox/interaction/triggering/data/TriggerResult.h"

#include "MoneyPickupPayload.generated.h"

struct FCollisionContext;

USTRUCT(BlueprintType)
struct FMoneyPickupPayload {
    GENERATED_BODY()

    FMoneyPickupPayload() = default;
    FMoneyPickupPayload(int32 quantity)
        : money(quantity) {}

    FTriggerResult trigger(FTriggerContext context);
    bool tick(float delta_time) { return false; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Money")
    int32 money{1};
};
