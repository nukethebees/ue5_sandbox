#pragma once

#include <SpaceGame/telemetry/LevelTelemetryReport.h>

#include <expected>

enum class ETelemetryDashboardMetric : uint8 {
    RequestedTimeScaleRatio,
    ObservedTimeScale,
    TicksPerRealSecond,
    RealSampleInterval,
    ActiveEntities,
    PlayerShips,
    Turrets,
    CapitalShips,
    CapitalShipFighters,
    TubeSpinners,
    ActiveLasers,
    RegistrySlots,
    OccupiedSpatialCells,
    SpawnRate,
    DestructionRate,
    KillRate,
    LaserFireRate,
    GridRebuildRate,
    RangeQueryRate,
    LineTraceRate,
    SweepTraceRate,
    COUNT,
};

struct SPACEGAME_API FTelemetryMetricSeries {
    ETelemetryDashboardMetric metric{ETelemetryDashboardMetric::RequestedTimeScaleRatio};
    FString title{};
    FString units{};
    TArray<float> real_elapsed_seconds{};
    TArray<float> values{};
    TOptional<double> weighted_mean{};
    bool uses_simulated_time{};
};

struct SPACEGAME_API FTelemetryRunAnalysis {
    TArray<float> throughput_real_elapsed_seconds{};
    TArray<float> observed_time_scale{};
    TArray<float> requested_time_scale{};
    TArray<FTelemetryMetricSeries> metrics{};
    TArray<float> battle_simulated_seconds{};
    TArray<float> battle_alive_entities{};
    TArray<float> battle_shots{};
    TArray<float> battle_hits{};
    TArray<float> battle_damage_dealt{};
    TArray<float> battle_kills{};

    [[nodiscard]] auto find_metric(ETelemetryDashboardMetric metric) const
        -> FTelemetryMetricSeries const*;
};

SPACEGAME_API auto analyze_level_telemetry_run(FLevelTelemetryReport const& record)
    -> FTelemetryRunAnalysis;
SPACEGAME_API auto telemetry_metric_title(ETelemetryDashboardMetric metric) -> FString;
SPACEGAME_API auto telemetry_metric_units(ETelemetryDashboardMetric metric) -> FString;

struct SPACEGAME_API FTelemetryRunSummary {
    FString path{};
    FString run_id{};
    FString map_name{};
    FName level_id{NAME_None};
    FString level_display_name{};
    FString launched_utc{};
    ELevelTelemetryRunEndReason completion_reason{ELevelTelemetryRunEndReason::WorldEnd};

    [[nodiscard]] auto level_label() const -> FString;
};

class SPACEGAME_API FTelemetryRunCatalog {
  public:
    explicit FTelemetryRunCatalog(FString directory = {});

    void refresh();
    void set_level_filter(FString level_label);
    bool select_run(FString const& run_id);
    bool select_baseline(FString const& run_id);

    [[nodiscard]] auto get_runs() const -> TConstArrayView<FTelemetryRunSummary> {
        return filtered_;
    }
    [[nodiscard]] auto get_level_filters() const -> TConstArrayView<FString> {
        return level_filters_;
    }
    [[nodiscard]] auto get_selected_run_id() const -> FString const& { return selected_run_id_; }
    [[nodiscard]] auto get_selected_record() const -> FLevelTelemetryReport const* {
        return selected_record_.IsSet() ? &selected_record_.GetValue() : nullptr;
    }
    [[nodiscard]] auto get_selected_error() const -> FString const& { return selected_error_; }
    [[nodiscard]] auto get_baseline_run_id() const -> FString const& { return baseline_run_id_; }
    [[nodiscard]] auto get_baseline_record() const -> FLevelTelemetryReport const* {
        return baseline_record_.IsSet() ? &baseline_record_.GetValue() : nullptr;
    }
    auto get_baseline_candidates() const -> TArray<FTelemetryRunSummary>;
    [[nodiscard]] auto get_unreadable_file_count() const noexcept -> int32 {
        return unreadable_file_count_;
    }
    [[nodiscard]] auto directory_exists() const noexcept -> bool { return directory_exists_; }
    [[nodiscard]] auto get_level_filter() const -> FString const& { return level_filter_; }
  private:
    void rebuild_filter();
    void load_selection();

    FString directory_{};
    TArray<FTelemetryRunSummary> all_{};
    TArray<FTelemetryRunSummary> filtered_{};
    TArray<FString> level_filters_{};
    FString level_filter_{TEXT("All levels")};
    FString selected_run_id_{};
    TOptional<FLevelTelemetryReport> selected_record_{};
    FString baseline_run_id_{};
    TOptional<FLevelTelemetryReport> baseline_record_{};
    FString selected_error_{};
    int32 unreadable_file_count_{};
    bool directory_exists_{};
};
