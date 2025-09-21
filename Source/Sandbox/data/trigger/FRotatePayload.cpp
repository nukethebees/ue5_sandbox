#include "FRotatePayload.h"

FTriggerResult FRotatePayload::trigger(FTriggerContext context) {
    logger.log_verbose(TEXT("Triggering rotation for actor %s"),
                       *context.triggered_actor.GetName());

    rotating_actor = &context.triggered_actor;
    start_rotation();

    FTriggerResult result{};
    result.enable_ticking = is_rotating;
    return result;
}

bool FRotatePayload::tick(float delta_time) {
    if (!is_rotating || time_remaining <= 0.0f || !rotating_actor.IsValid()) {
        return false; // Stop ticking
    }

    // Apply rotation
    auto* const actor{rotating_actor.Get()};
    auto const new_rotation{actor->GetActorRotation() + rotation_rate * delta_time};
    actor->SetActorRotation(new_rotation);

    // Update timer
    time_remaining -= delta_time;

    if (time_remaining <= 0.0f) {
        stop_rotation();
        return false; // Stop ticking
    }

    return true; // Continue ticking
}

void FRotatePayload::start_rotation() {
    if (!is_rotating) {
        is_rotating = true;
        time_remaining = rotation_duration_seconds;
        logger.log_verbose(TEXT("Started rotation for %.1f seconds"), rotation_duration_seconds);
    }
}

void FRotatePayload::stop_rotation() {
    if (is_rotating) {
        is_rotating = false;
        time_remaining = 0.0f;
        logger.log_verbose(TEXT("Stopped rotation"));
    }
}
