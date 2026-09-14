#pragma once

#include "ioj/sim/entity_overlaps.h"

#include <cstdint>
#include <vector>

namespace ioj::sim::collision {
void sort_and_deduplicate(ioj::sim::collision::EntityEntityOverlaps& overlaps,
                          std::vector<std::int32_t>& sort_indices_scratch);
void sort_and_deduplicate(ioj::sim::collision::EntityStaticOverlaps& overlaps,
                          std::vector<std::int32_t>& sort_indices_scratch);
} // namespace ioj::sim::collision
