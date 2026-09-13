#pragma once

#include <cstdint>

namespace ml::simulation {
enum class SpaceShipFlightMode : std::uint8_t {
    ForwardSpeed,
    PlanarVelocity,
};
}
