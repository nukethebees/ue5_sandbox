#pragma once

#include <sandbox/simulation/ship_health.h>
#include <SpaceGameSimulation/ships/common/ShipHealth.h>

namespace ml {
inline auto to_native(FShipHealth const value) noexcept -> simulation::ShipHealth {
    return {value.health, value.max_health};
}
inline auto to_unreal(simulation::ShipHealth const value) noexcept -> FShipHealth {
    return {value.health, value.max_health};
}
}
