#pragma once

#include "ShooterGame/interaction/StrongIds.h"
#include "ShooterGame/interaction/TriggerableId.h"
#include "ShooterGame/interaction/TriggerContext.h"
#include "ShooterGame/interaction/TriggerResult.h"
#include "SandboxGameShared/logging/LogMsgMixin.hpp"

#include <array>

#include "CoreMinimal.h"

struct FTriggerOtherPayload : public ml::LogMsgMixin<"FTriggerOtherPayload"> {
    static constexpr uint8 MAX_TARGETS{8};

    std::array<ActorId, MAX_TARGETS> target_actor_ids{};
    uint8 n_targets{0};
    float activation_delay{0.0f};

    [[nodiscard]] bool add_target_actor(ActorId actor_id);

    FTriggerResult trigger(FTriggerContext context);
    bool tick(float delta_time) { return false; } // No ticking needed
};
