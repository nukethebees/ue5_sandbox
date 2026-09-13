#pragma once

#include <cstdint>

namespace ml::simulation {
enum class LevelTelemetryRunEndReason : std::uint8_t {
    MissionSucceeded,
    MissionFailed,
    BattleResolved,
    DurationReached,
    OrchestratorReset,
    WorldEnd,
};
}
