#pragma once

#include "ioj/sim/collision/entity_overlaps.h"

namespace ioj::sim::collision {
struct DetectedOverlapsView {
    EntityEntityOverlaps::CompactConstView entity_entity_overlaps;
    EntityStaticOverlaps::CompactConstView entity_static_overlaps;
};
} // namespace ioj::sim::collision
