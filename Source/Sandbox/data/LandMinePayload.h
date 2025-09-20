#pragma once

#include "Sandbox/data/CollisionContext.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

struct FLandMinePayload : public ml::LogMsgMixin<"FLandMinePayload"> {
    FLandMinePayload() = default;
    FLandMinePayload(float damage,
                     float radius = 100.0f,
                     float force = 1000.0f,
                     FVector location = FVector::ZeroVector)
        : damage(damage)
        , explosion_radius(radius)
        , explosion_force(force)
        , mine_location(location) {}

    void execute(FCollisionContext context);

    float damage{25.0f};
    float explosion_radius{100.0f};
    float explosion_force{1000.0f};
    FVector mine_location{FVector::ZeroVector};
};
