#pragma once

#include "attack_distance_band_settings.h"
#include <ioj/sim/levels/simulation_settings.h>

namespace ioj::s7 {
struct SimulationSettingsInput {
    explicit SimulationSettingsInput(sim::SimulationSettings& settings)
        : values{settings}
        , attack_distance_band{settings.fighters.attack_distance_band.minimum(),
                               settings.fighters.attack_distance_band.desired(),
                               settings.fighters.attack_distance_band.maximum()} {}

    sim::SimulationSettings& values;
    AttackDistanceBandSettings attack_distance_band;
};
}
