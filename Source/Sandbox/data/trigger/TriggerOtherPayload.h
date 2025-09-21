#pragma once

#include <array>

#include "CoreMinimal.h"
#include "Sandbox/data/trigger/TriggerableId.h"
#include "Sandbox/data/trigger/TriggerContext.h"
#include "Sandbox/data/trigger/TriggerResult.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

// Forward declare ActorId
using ActorId = uint64;

struct FTriggerOtherPayload : public ml::LogMsgMixin<"FTriggerOtherPayload"> {
    static constexpr uint8 MAX_TARGETS{8};

    std::array<ActorId, MAX_TARGETS> target_actor_ids{};
    uint8 n_targets{0};
    float activation_delay{0.0f};

    void add_target_actor(ActorId actor_id) {
        if (n_targets < MAX_TARGETS && actor_id != 0) {
            target_actor_ids[n_targets++] = actor_id;
        }
    }

    // Legacy method for backward compatibility
    void add_target(TriggerableId target) {
        // This method is deprecated - use add_target_actor instead
        // For now, we'll ignore this to force migration to actor IDs
    }

    FTriggerResult trigger(FTriggerContext context);
    bool tick(float delta_time) { return false; } // No ticking needed
};
