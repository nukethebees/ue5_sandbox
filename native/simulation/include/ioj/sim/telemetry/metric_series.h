#pragma once

#include <ioj/sim/telemetry/metric.h>

#include <optional>
#include <vector>

namespace ioj::sim::telemetry {
struct MetricSeries {
    Metric metric{Metric::ActiveEntities};
    std::vector<float> simulated_elapsed_seconds;
    std::vector<float> values;
    std::optional<double> weighted_mean;
};
}
