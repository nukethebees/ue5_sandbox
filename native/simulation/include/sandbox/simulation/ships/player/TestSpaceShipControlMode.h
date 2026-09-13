#pragma once

#include <cstdint>

namespace ml::simulation {
enum class SpaceShipControlMode : std::uint8_t {
    Velocity,
    Power,
    COUNT,
};
}
