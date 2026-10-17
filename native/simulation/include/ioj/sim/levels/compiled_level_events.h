#pragma once

#include <ioj/sim/levels/level_event_schedule.h>
#include <ioj/sim/levels/level_initialisation_data.h>

namespace ioj::sim {
struct CompiledLevelEvents {
    LevelInitialisationData initialisation{};
    LevelEventSchedule schedule{};
};

}
