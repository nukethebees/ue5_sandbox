#pragma once

#include <cstdint>

namespace ioj::sim {
enum class SpaceShipFlightMode : std::uint8_t {
    ForwardSpeed,
    PlanarVelocity,
};
}
