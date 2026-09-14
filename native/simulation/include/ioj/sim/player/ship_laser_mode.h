#pragma once

#include <cstdint>

namespace ioj::sim {
enum class ShipLaserMode : std::uint8_t {
    Single,
    Double,
    Hyper,
};
}
