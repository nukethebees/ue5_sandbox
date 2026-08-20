#pragma once

#include "ShooterGame/interaction/StrongIds.h"
#include "ShooterGame/interaction/TriggerableId.h"
#include "ShooterGame/interaction/TriggerContext.h"
#include "ShooterGame/interaction/TriggerResult.h"

#include "CoreMinimal.h"

#include "MoneyPickupPayload.generated.h"

struct FCollisionContext;

USTRUCT(BlueprintType)
struct SHOOTERGAME_API FMoneyPickupPayload {
    GENERATED_BODY()

    FMoneyPickupPayload() = default;
    FMoneyPickupPayload(int32 quantity)
        : money(quantity) {}

    FTriggerResult trigger(FTriggerContext context);
    bool tick(float delta_time) { return false; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Money")
    int32 money{1};
};
