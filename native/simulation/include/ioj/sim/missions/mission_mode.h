#pragma once

#include <cstdint>

namespace ioj::sim {
enum class MissionMode : std::uint8_t {
    None,
    SurviveTime,
    KillEnemies,
    KillEnemiesWithinTime,
};
}
