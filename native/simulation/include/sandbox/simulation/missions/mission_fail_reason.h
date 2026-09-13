#pragma once

#include <cstdint>

namespace ml::simulation {
enum class MissionFailReason : std::uint8_t {
    None,
    PlayerKilled,
    TimeElapsed,
    DefenceObjectiveFailed,
};
}
