#pragma once

#include <ioj/sim/telemetry/run_completion.h>
#include <ioj/sim/telemetry/run_data.h>
#include <ioj/sim/telemetry/run_metadata.h>

#include <cstdint>
#include <vector>

namespace ioj::sim {
struct LevelTelemetryRunRecord {
    static constexpr std::int32_t schema_version{4};

    std::int32_t loaded_schema_version{schema_version};
    LevelTelemetryRunMetadata metadata{};
    LevelTelemetryRunCompletion completion{};
    LevelTelemetryTickSeries tick_series{};
    std::vector<LevelTelemetryBattleSample> battle_samples{};
};
}
