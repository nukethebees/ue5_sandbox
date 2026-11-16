#pragma once

#include <ioj/sim/levels/level_read_access.h>

namespace ioj::sim {
class LevelEventManager;
}

namespace ioj::sim::tasks {
struct TaskAllocationContext {
    LevelReadAccess level;
    LevelEventManager const& events;
};
}
