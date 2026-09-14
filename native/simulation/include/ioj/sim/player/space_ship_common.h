#pragma once

#include <cstdint>

namespace ioj::sim::player {
enum class BoostBrakeState : std::uint8_t { None, Boost, Brake };
}
