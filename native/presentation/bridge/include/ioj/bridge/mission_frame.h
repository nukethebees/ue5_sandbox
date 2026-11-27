#pragma once

#include <ioj/presentation/mission_frame.h>
#include <ioj/presentation/mission_init.h>

namespace ioj::session {
struct MissionLaunchRequest;
}

namespace ioj::sim {
struct LevelSim;
}
namespace ioj::bridge {
auto mission_init(session::MissionLaunchRequest const& mission, sim::LevelSim const& level)
    -> presentation::MissionInit;
auto mission_frame(sim::LevelSim const& level)
    -> presentation::MissionFrame;
}
