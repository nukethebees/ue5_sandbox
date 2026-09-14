#pragma once

#include <cstdint>

namespace ioj::sim {
enum class MissionFailReason : std::uint8_t {
    None,
    PlayerKilled,
    TimeElapsed,
    DefenceObjectiveFailed,
};
}
