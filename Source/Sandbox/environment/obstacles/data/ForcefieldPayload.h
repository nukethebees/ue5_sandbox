#pragma once

#include "CoreMinimal.h"
#include "Sandbox/interaction/triggering/data/TriggerContext.h"
#include "Sandbox/interaction/triggering/data/TriggerResult.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"

#include "ForcefieldPayload.generated.h"

USTRUCT(BlueprintType)
struct FForcefieldPayload {
    GENERATED_BODY()

    // Runtime state
    TWeakObjectPtr<AActor> forcefield_actor{nullptr};

    // Static logger since can't inherit from LogMsgMixin
    static inline auto logger{ml::LogMsgMixin<"FForcefieldPayload">{}};

    FTriggerResult trigger(FTriggerContext context);
    bool tick(float delta_time);
};
