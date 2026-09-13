#pragma once

#include "sandbox/simulation/entity_overlaps.h"

#include <cstdint>
#include <vector>

namespace ml::simulation::collision {
void sort_and_deduplicate(ioj::FEntityEntityOverlaps& overlaps,
                          std::vector<std::int32_t>& sort_indices_scratch);
void sort_and_deduplicate(ioj::FEntityStaticOverlaps& overlaps,
                          std::vector<std::int32_t>& sort_indices_scratch);
} // namespace ml::simulation::collision
