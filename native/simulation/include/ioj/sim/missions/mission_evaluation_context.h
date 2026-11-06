#pragma once

#include <ioj/levels/team.h>
#include <ioj/sim/sim_tick.h>

#include <span>

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
struct MissionEvaluationContext {
    SimTick tick{};
    FrameMemoryResource* scratch{};
    std::span<Team const> participating_teams;
};
}
