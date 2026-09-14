#pragma once

#include <ioj/sim/ship_health.h>
#include <SpaceGameSimulation/ships/common/ShipHealth.h>

namespace ml {
inline auto to_native(FShipHealth const value) noexcept -> ::ioj::sim::ShipHealth {
    return {value.health, value.max_health};
}
inline auto to_unreal(::ioj::sim::ShipHealth const value) noexcept -> FShipHealth {
    return {value.health, value.max_health};
}
}
