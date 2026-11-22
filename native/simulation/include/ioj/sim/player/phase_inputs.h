#pragma once
#include <ioj/sim/player/player_events.h>

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
class DamageQueue;
class HealthTable;
struct EntityDeathInfo;
}

namespace ioj::sim::lasers {
struct Sim;
}

namespace ioj::sim::player {
struct ActionInputs {
    lasers::Sim& laser_simulation;
    PlayerEvents& events;
};

struct ResolutionInputs {
    HealthTable& health_table;
    DamageQueue const& damage_queue;
    EntityDeathInfo& deaths;
};
} // namespace ioj::sim::player
