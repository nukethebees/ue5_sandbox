#pragma once

#include <concepts>

#include "Sandbox/data/TriggerContext.h"

namespace ml {
template <typename T>
concept IsTriggerPayload = requires(T payload, FTriggerContext context) {
    { payload.trigger(context) } -> std::same_as<void>;
};
}
