#pragma once

#include <ioj/frequency.h>
#include <ioj/sim/telemetry/run_completion.h>
#include <ioj/sim/telemetry/run_data.h>
#include <ioj/sim/telemetry/run_metadata.h>

#include <cstdint>
#include <vector>

namespace ioj::sim::telemetry {
struct RunRecord {
    static constexpr std::int32_t schema_version{11};

    RunMetadata metadata{};
    Frequency tick_rate_hz{Frequency::make(60.0)};
    RunCompletion completion{};
    TickSeries tick_series{};
    std::vector<BattleSample> battle_samples{};
};
}
