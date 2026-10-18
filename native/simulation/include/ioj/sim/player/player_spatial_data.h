#pragma once

#include <ioj/rotator_types.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/health.h>
#include <ioj/vector_types.h>

namespace ioj::sim {
struct PlayerSpatialData {
    EntityId id;
    Vector3f location{};
    Vector3f velocity{};
    Quaternion4f orientation{};
    Health health{};
};
}
