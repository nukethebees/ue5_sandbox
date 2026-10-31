#pragma once

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
class CombatEvents;
struct SimClock;
}

namespace ioj::sim::capital_ships {
struct ResolutionInputs {
    CombatEvents const& combat_events;
    SimClock const& clock;
    FrameMemoryResource& scratch_resource;
};
} // namespace ioj::sim::capital_ships
