#pragma once

#include <ioj/math_types.h>
#include <ioj/rotator_types.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/health.h>

namespace ioj::sim {
struct PlayerSpatialData {
    EntityId id;
    Vector3f location{};
    Vector3f velocity{};
    Quaternion4f orientation{};
    Health health{};
};
}
