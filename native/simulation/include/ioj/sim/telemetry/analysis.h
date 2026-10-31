#pragma once

#include <ioj/sim/telemetry/analysis_input.h>
#include <ioj/sim/telemetry/metric_series.h>

#include <vector>

namespace ioj::sim::telemetry {
struct Analysis {
    std::vector<MetricSeries> metrics;
    std::vector<float> battle_simulated_seconds;
    std::vector<float> battle_alive_entities;
    std::vector<float> battle_shots;
    std::vector<float> battle_hits;
    std::vector<float> battle_damage_dealt;
    std::vector<float> battle_kills;
};

[[nodiscard]] auto analyze(AnalysisInput input) -> Analysis;
}
