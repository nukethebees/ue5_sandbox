#pragma once

#include "Sandbox/interaction/StrongIds.h"
#include "Sandbox/interaction/TriggerableId.h"
#include "Sandbox/interaction/TriggerContext.h"
#include "Sandbox/interaction/TriggerResult.h"

#include "CoreMinimal.h"

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
