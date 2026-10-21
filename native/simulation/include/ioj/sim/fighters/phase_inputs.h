#pragma once

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
class CombatEvents;
struct SpatialQueryManager;
}

namespace ioj::sim::lasers {
struct Sim;
}

namespace ioj::sim::fighters {
struct ThinkingInputs {
    float dt;
    SpatialQueryManager const& spatial_query_manager;
    FrameMemoryResource& scratch_resource;
};

struct ActionInputs {
    lasers::Sim& laser_simulation;
    FrameMemoryResource& scratch_resource;
};

struct ResolutionInputs {
    CombatEvents const& combat_events;
    FrameMemoryResource& scratch_resource;
};
} // namespace ioj::sim::fighters
