#pragma once

#include <ioj/frequency.h>
#include <ioj/sim/telemetry/run_completion.h>
#include <ioj/sim/telemetry/run_data.h>
#include <ioj/sim/telemetry/run_metadata.h>

#include <cstdint>
#include <vector>

namespace ioj::sim {
struct LevelTelemetryRunRecord {
    static constexpr std::int32_t schema_version{8};

    std::int32_t loaded_schema_version{schema_version};
    LevelTelemetryRunMetadata metadata{};
    Frequency tick_rate_hz{Frequency::make(60.0)};
    LevelTelemetryRunCompletion completion{};
    LevelTelemetryTickSeries tick_series{};
    std::vector<LevelTelemetryBattleSample> battle_samples{};
};
}
