#pragma once

#include <cstdint>

namespace ml::simulation {
enum class ShipFireRate : std::uint8_t {
    Single,
    Burst3,
    FullAuto,
    COUNT,
};
}
