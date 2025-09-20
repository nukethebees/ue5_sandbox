#pragma once

#include "Sandbox/data/CollisionContext.h"

struct FLandMinePayload {
    FLandMinePayload() = default;
    FLandMinePayload(float damage)
        : damage(damage) {}

    void execute(FCollisionContext context);

    float damage{25.0f};
};
