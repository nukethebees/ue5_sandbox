#pragma once

#include "CoreMinimal.h"
#include "Sandbox/data/trigger/TriggerContext.h"
#include "Sandbox/data/trigger/TriggerResult.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

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
