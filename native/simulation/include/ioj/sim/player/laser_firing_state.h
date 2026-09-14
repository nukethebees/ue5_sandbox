#pragma once

#include <cstdint>

namespace ioj::sim {
enum class LaserFiringState : std::uint8_t {
    idle,
    burst,
    lock_on_transition,
    lock_on_searching,
    lock_on_acquired,
};
}
