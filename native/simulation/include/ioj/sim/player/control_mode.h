#pragma once

#include <cstdint>

namespace ioj::sim {
enum class SpaceShipControlMode : std::uint8_t {
    Velocity,
    Power,
    COUNT,
};
}
