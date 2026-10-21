#pragma once

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
class CombatEvents;
}

namespace ioj::sim::lasers {
struct Sim;
}

namespace ioj::sim::player {
struct ActionInputs {
    lasers::Sim& laser_simulation;
};

struct ResolutionInputs {
    CombatEvents const& combat_events;
    FrameMemoryResource& scratch_resource;
};
} // namespace ioj::sim::player
