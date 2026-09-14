#pragma once

#include <cstdint>

namespace ioj::sim {
enum class LevelTelemetryRunEndReason : std::uint8_t {
    MissionSucceeded,
    MissionFailed,
    BattleResolved,
    DurationReached,
    OrchestratorReset,
    WorldEnd,
};
}
