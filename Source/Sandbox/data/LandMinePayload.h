#pragma once

#include "Sandbox/data/CollisionContext.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "TimerManager.h"

struct FLandMinePayload : public ml::LogMsgMixin<"FLandMinePayload"> {
    FLandMinePayload() = default;
    FLandMinePayload(float damage,
                     float radius = 100.0f,
                     float force = 1000.0f,
                     FVector location = FVector::ZeroVector,
                     float delay = 2.0f)
        : damage(damage)
        , explosion_radius(radius)
        , explosion_force(force)
        , mine_location(location)
        , detonation_delay(delay) {}

    void execute(FCollisionContext context);
    void explode(FCollisionContext context);
    FVector calculate_impulse(FVector target_location, float target_distance);

    float damage{25.0f};
    float explosion_radius{100.0f};
    float explosion_force{5000.0f};
    FVector mine_location{FVector::ZeroVector};
    float detonation_delay{2.0f};
  private:
    FTimerHandle timer_handle{};
};
