#pragma once

#include "Sandbox/interaction/TriggerContext.h"
#include "Sandbox/interaction/TriggerResult.h"

#include <concepts>

namespace ml {
template <typename T>
concept IsTriggerPayload = requires(T payload, FTriggerContext context, float dt) {
    { payload.trigger(context) } -> std::same_as<FTriggerResult>;
    { payload.tick(dt) } -> std::same_as<bool>;
};
}
