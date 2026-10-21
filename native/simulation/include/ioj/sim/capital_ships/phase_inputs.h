#pragma once

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
class CombatEvents;
struct SimClock;
struct SpatialQueryManager;
}

namespace ioj::sim::capital_ships {
struct ThinkingInputs {
    SpatialQueryManager const& spatial_query_manager;
    FrameMemoryResource& scratch_resource;
};

struct ResolutionInputs {
    CombatEvents const& combat_events;
    SimClock const& clock;
    FrameMemoryResource& scratch_resource;
};
} // namespace ioj::sim::capital_ships
