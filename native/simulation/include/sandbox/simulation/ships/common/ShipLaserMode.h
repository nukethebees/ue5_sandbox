#pragma once

#include <cstdint>

namespace ml::simulation {
enum class ShipLaserMode : std::uint8_t {
    Single,
    Double,
    Hyper,
};
}
