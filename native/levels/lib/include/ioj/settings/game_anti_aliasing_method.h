#pragma once

#include <cstdint>

namespace ioj {
enum class GameAntiAliasingMethod : std::uint8_t {
    Off,
    FXAA,
    TAA,
    TSR,
    SMAA,
};
}
