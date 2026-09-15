#pragma once

#include <ioj/sim/player/sim.h>

namespace ioj::sim {
struct PlayerSimTestAccess {
    static void set_transform(player::Sim& simulation, Transform3d const& transform) {
        simulation.movement_state_.transform = transform;
    }
};
}
