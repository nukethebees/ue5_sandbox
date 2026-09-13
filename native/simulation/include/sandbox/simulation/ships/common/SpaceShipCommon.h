#pragma once

#include <cstdint>

namespace ml::simulation::player {
enum class BoostBrakeState : std::uint8_t { None, Boost, Brake };
}
