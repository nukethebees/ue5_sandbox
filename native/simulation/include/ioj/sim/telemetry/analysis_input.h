#pragma once

#include <ioj/sim/telemetry/run_record.h>

#include <span>

namespace ioj::sim::telemetry {
struct AnalysisInput {
    Frequency tick_rate_hz{Frequency::make(60.0)};
    std::span<BattleSample const> battle_samples;
};
}
