#pragma once

#include <cstdint>

namespace ioj::sim {
enum class ShipFireRate : std::uint8_t {
    Single,
    Burst3,
    FullAuto,
    COUNT,
};
}
