#pragma once

#include <concepts>

#include "Sandbox/interaction/triggering/data/TriggerContext.h"
#include "Sandbox/interaction/triggering/data/TriggerResult.h"

namespace ml {
template <typename T>
concept IsTriggerPayload = requires(T payload, FTriggerContext context, float dt) {
    { payload.trigger(context) } -> std::same_as<FTriggerResult>;
    { payload.tick(dt) } -> std::same_as<bool>;
};
}
