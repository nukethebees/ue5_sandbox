#pragma once

#include <ioj/sim/telemetry/analysis.h>
#include <ioj/sim/telemetry/report.h>
#include <ioj/sim/telemetry/run_catalog.h>
#include <ioj/sim/telemetry/run_end_reason.h>

#include <CoreMinimal.h>

struct SPACEGAME_API FTelemetryMetricSeries {
    ioj::sim::telemetry::Metric metric{ioj::sim::telemetry::Metric::ActiveEntities};
    FString title{};
    FString units{};
    TArray<float> simulated_elapsed_seconds{};
    TArray<float> values{};
    TOptional<double> weighted_mean{};
};

struct SPACEGAME_API FTelemetryRunAnalysis {
    TArray<FTelemetryMetricSeries> metrics{};
    TArray<float> battle_simulated_seconds{};
    TArray<float> battle_alive_entities{};
    TArray<float> battle_shots{};
    TArray<float> battle_hits{};
    TArray<float> battle_damage_dealt{};
    TArray<float> battle_kills{};

    [[nodiscard]] auto find_metric(ioj::sim::telemetry::Metric metric) const
        -> FTelemetryMetricSeries const*;
};

SPACEGAME_API auto analyze_level_telemetry_run(ioj::sim::LevelTelemetryReport const& record)
    -> FTelemetryRunAnalysis;
SPACEGAME_API auto telemetry_metric_title(ioj::sim::telemetry::Metric metric) -> FString;
SPACEGAME_API auto telemetry_metric_units(ioj::sim::telemetry::Metric metric) -> FString;

struct FTelemetryRunSummary {
    FString run_id{};
    FString level_label{};
    FString launched_utc{};
    ioj::sim::LevelTelemetryRunEndReason completion_reason{};
};
