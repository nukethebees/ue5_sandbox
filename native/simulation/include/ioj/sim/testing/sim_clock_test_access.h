#pragma once

#include <ioj/sim/sim_clock.h>

namespace ioj::sim {
struct SimClockTestAccess {
    static void set_phase(SimClock& clock, SimulationPhase const phase) noexcept {
        clock.phase_ = phase;
    }
};
}
