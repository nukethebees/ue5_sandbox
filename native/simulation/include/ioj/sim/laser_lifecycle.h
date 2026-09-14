#pragma once

#include "ioj/sim/laser_soa.h"

#include <memory_resource>
#include <span>

namespace ioj::sim::lasers {
void
    expire_instances(Entities& entities, float delta_time, std::pmr::memory_resource& frame_memory);
void update_locations(EntitiesView entities, float delta_time);
void remove_instances(Entities& entities, std::span<std::int32_t const> indices);
} // namespace ioj::sim::lasers
