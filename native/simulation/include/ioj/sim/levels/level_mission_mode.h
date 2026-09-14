#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace ioj::sim::levels {
enum class LevelMissionMode : std::uint8_t {
    Unspecified,
    SurviveTime,
    KillEnemies,
    KillEnemiesWithinTime,
};
} // namespace ioj::sim::levels
