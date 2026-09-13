#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace ml {
enum class ELevelMissionMode : std::uint8_t {
    Unspecified,
    SurviveTime,
    KillEnemies,
    KillEnemiesWithinTime,
};
}
