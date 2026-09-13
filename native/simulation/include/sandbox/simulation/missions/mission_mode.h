#pragma once

#include <cstdint>

namespace ml::simulation {
enum class MissionMode : std::uint8_t {
    None,
    SurviveTime,
    KillEnemies,
    KillEnemiesWithinTime,
};
}
