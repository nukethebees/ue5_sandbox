#include <SpaceGame/telemetry/TelemetryDashboardModel.h>

#include <SandboxCoreEngine/strings.h>

auto telemetry_metric_title(ioj::sim::telemetry::Metric const metric) -> FString {
    return ioj::to_fstring(ioj::sim::telemetry::metric_title(metric));
}
auto telemetry_metric_units(ioj::sim::telemetry::Metric const metric) -> FString {
    return ioj::to_fstring(ioj::sim::telemetry::metric_units(metric));
}

auto FTelemetryRunAnalysis::find_metric(ioj::sim::telemetry::Metric const metric) const
    -> FTelemetryMetricSeries const* {
    return metrics.FindByPredicate([metric](auto const& value) { return value.metric == metric; });
}

auto analyze_level_telemetry_run(ioj::sim::telemetry::Report const& record)
    -> FTelemetryRunAnalysis {
    auto const native{::ioj::sim::telemetry::analyze({
        .tick_rate_hz = record.run.tick_rate_hz,
        .battle_samples = record.run.battle_samples,
    })};

    FTelemetryRunAnalysis result;
    result.metrics.Reserve(static_cast<int32>(native.metrics.size()));
    for (auto const& metric : native.metrics) {
        FTelemetryMetricSeries series{
            .metric = metric.metric,
            .title = telemetry_metric_title(metric.metric),
            .units = telemetry_metric_units(metric.metric),
        };
        if (metric.weighted_mean) {
            series.weighted_mean = *metric.weighted_mean;
        }
        series.simulated_elapsed_seconds.Append(
            metric.simulated_elapsed_seconds.data(),
            static_cast<int32>(metric.simulated_elapsed_seconds.size()));
        series.values.Append(metric.values.data(), static_cast<int32>(metric.values.size()));
        result.metrics.Add(MoveTemp(series));
    }
    auto const append{[](TArray<float>& destination, std::vector<float> const& source) {
        destination.Append(source.data(), static_cast<int32>(source.size()));
    }};
    append(result.battle_simulated_seconds, native.battle_simulated_seconds);
    append(result.battle_alive_entities, native.battle_alive_entities);
    append(result.battle_shots, native.battle_shots);
    append(result.battle_hits, native.battle_hits);
    append(result.battle_damage_dealt, native.battle_damage_dealt);
    append(result.battle_kills, native.battle_kills);
    return result;
}
