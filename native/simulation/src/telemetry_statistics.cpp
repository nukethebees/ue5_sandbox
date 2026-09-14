#include "ioj/sim/telemetry_statistics.h"

#include <algorithm>
#include <cmath>

namespace ioj::sim::telemetry {
auto aggregate_timings(std::span<double> const samples) noexcept -> LevelTelemetryTimingAggregate {
    LevelTelemetryTimingAggregate result;
    result.sample_count = samples.size();
    if (samples.empty()) {
        return result;
    }

    double total{};
    for (auto const sample : samples) {
        total += sample;
        result.max_ms = std::max(result.max_ms, sample * 1000.0);
    }
    std::sort(samples.begin(), samples.end());

    auto const p95_index{std::min(static_cast<std::size_t>(std::ceil(samples.size() * 0.95) - 1),
                                  samples.size() - 1)};
    result.mean_ms = total * 1000.0 / static_cast<double>(samples.size());
    result.p95_ms = samples[p95_index] * 1000.0;
    return result;
}
} // namespace ioj::sim::telemetry
