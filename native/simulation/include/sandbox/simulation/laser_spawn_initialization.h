#pragma once

#include "sandbox/simulation/laser_soa.h"

namespace ml::simulation::lasers {
void initialise_spawns(Entities& entities,
                       SpawnRequestsConstView requests,
                       float tick_period,
                       float simulation_time,
                       float fixed_spawn_offset);
} // namespace ml::simulation::lasers
