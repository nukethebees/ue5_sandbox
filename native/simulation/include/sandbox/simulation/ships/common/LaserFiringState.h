#pragma once

#include <cstdint>

namespace ml::simulation {
enum class LaserFiringState : std::uint8_t {
    idle,
    burst,
    lock_on_transition,
    lock_on_searching,
    lock_on_acquired,
};
}
