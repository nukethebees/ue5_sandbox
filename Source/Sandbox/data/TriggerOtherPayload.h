#pragma once

#include <array>

#include "CoreMinimal.h"
#include "Sandbox/data/TriggerableId.h"
#include "Sandbox/data/TriggerContext.h"
#include "Sandbox/data/TriggerResult.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

struct FTriggerOtherPayload : public ml::LogMsgMixin<"FTriggerOtherPayload"> {
    static constexpr uint8 MAX_TARGETS{8};

    std::array<TriggerableId, MAX_TARGETS> targets{};
    uint8 n_targets{0};
    float activation_delay{0.0f};

    void add_target(TriggerableId target) {
        if (n_targets < MAX_TARGETS && target.is_valid()) {
            targets[n_targets++] = target;
        }
    }

    FTriggerResult trigger(FTriggerContext context);
    bool tick(float delta_time) { return false; } // No ticking needed
};
