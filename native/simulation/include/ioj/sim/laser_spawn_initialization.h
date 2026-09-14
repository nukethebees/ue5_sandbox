#pragma once

#include "ioj/sim/laser_soa.h"

namespace ioj::sim::lasers {
void initialise_spawns(Entities& entities,
                       SpawnRequestsConstView requests,
                       float tick_period,
                       float simulation_time,
                       float fixed_spawn_offset);
} // namespace ioj::sim::lasers
