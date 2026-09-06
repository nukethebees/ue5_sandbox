#include <SpaceGame/telemetry/TelemetryDashboardModel.h>

#include <SpaceGame/telemetry/LevelTelemetryJson.h>

#include <Algo/Sort.h>
#include <Dom/JsonObject.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>
#include <Serialization/JsonSerializer.h>

namespace telemetry_dashboard {
auto read_summary(FString const& path) -> std::expected<FTelemetryRunSummary, FString> {
    FString json;
    if (!FFileHelper::LoadFileToString(json, *path)) {
        return std::unexpected{FString::Printf(TEXT("Could not read '%s'"), *path)};
    }
    TSharedPtr<FJsonObject> root;
    auto reader{TJsonReaderFactory<>::Create(json)};
    if (!FJsonSerializer::Deserialize(reader, root) || !root.IsValid()) {
        return std::unexpected{FString::Printf(TEXT("Malformed JSON in '%s'"), *path)};
    }
    double schema{};
    if (!root->TryGetNumberField(TEXT("schema_version"), schema) ||
        schema != FLevelTelemetryRunRecord::schema_version) {
        return std::unexpected{
            FString::Printf(TEXT("Unsupported or missing schema in '%s'"), *path)};
    }
    TSharedPtr<FJsonObject> const* level{};
    TSharedPtr<FJsonObject> const* timestamps{};
    TSharedPtr<FJsonObject> const* completion{};
    if (!root->TryGetObjectField(TEXT("level"), level) || !level || !level->IsValid() ||
        !root->TryGetObjectField(TEXT("timestamps"), timestamps) || !timestamps ||
        !timestamps->IsValid() || !root->TryGetObjectField(TEXT("completion"), completion) ||
        !completion || !completion->IsValid()) {
        return std::unexpected{FString::Printf(TEXT("Summary objects are missing in '%s'"), *path)};
    }
    FTelemetryRunSummary result{.path = path};
    FString level_id;
    FString reason;
    if (!root->TryGetStringField(TEXT("run_id"), result.run_id) ||
        !(*level)->TryGetStringField(TEXT("map_name"), result.map_name) ||
        !(*level)->TryGetStringField(TEXT("level_id"), level_id) ||
        !(*level)->TryGetStringField(TEXT("display_name"), result.level_display_name) ||
        !(*timestamps)->TryGetStringField(TEXT("launched_utc"), result.launched_utc) ||
        !(*completion)->TryGetStringField(TEXT("reason"), reason)) {
        return std::unexpected{FString::Printf(TEXT("Summary fields are missing in '%s'"), *path)};
    }
    result.level_id = FName{level_id};
    if (!ml::try_parse_serialized(FStringView{reason}, result.completion_reason)) {
        return std::unexpected{FString::Printf(TEXT("Unknown completion reason in '%s'"), *path)};
    }
    return result;
}

template <typename Series>
auto as_of(Series const& series, uint64 const tick) -> typename Series::value_type const* {
    auto const count{series.num()};
    for (int32 index{count - 1}; index >= 0; --index) {
        if (series.time_at(index) <= tick) {
            return &series.value_at(index);
        }
    }
    return nullptr;
}

auto requested_unchanged(FLevelTelemetryTickSeries::DoubleData const& series,
                         uint64 const begin_tick,
                         uint64 const end_tick,
                         double& output) -> bool {
    auto const* const begin{as_of(series, begin_tick)};
    auto const* const end{as_of(series, end_tick)};
    if (!begin || !end || *begin <= 0.0 || !FMath::IsFinite(*begin) || *begin != *end) {
        return false;
    }
    auto const count{series.num()};
    for (int32 index{}; index < count; ++index) {
        auto const tick{series.time_at(index)};
        if (tick > begin_tick && tick <= end_tick && series.value_at(index) != *begin) {
            return false;
        }
    }
    output = *begin;
    return true;
}

struct FMetricBuilder {
    FTelemetryMetricSeries series;
    double weighted_sum{};
    double total_weight{};

    explicit FMetricBuilder(ETelemetryDashboardMetric const metric)
        : series{.metric = metric,
                 .title = telemetry_metric_title(metric),
                 .units = telemetry_metric_units(metric)} {}

    void add(double const time, double const value, double const weight) {
        if (!FMath::IsFinite(time) || !FMath::IsFinite(value) || weight <= 0.0 ||
            !FMath::IsFinite(weight)) {
            return;
        }
        series.real_elapsed_seconds.Add(static_cast<float>(time));
        series.values.Add(static_cast<float>(value));
        weighted_sum += value * weight;
        total_weight += weight;
    }

    auto finish() -> FTelemetryMetricSeries {
        if (total_weight > 0.0) {
            series.weighted_mean = weighted_sum / total_weight;
        }
        return MoveTemp(series);
    }
};

template <typename Series>
void add_gauge(FMetricBuilder& builder,
               Series const& series,
               uint64 const tick,
               double const time,
               double const weight) {
    auto const* const value{as_of(series, tick)};
    if (value) {
        builder.add(time, static_cast<double>(*value), weight);
    }
}

template <typename Series>
void add_rate(FMetricBuilder& builder,
              Series const& series,
              uint64 const begin_tick,
              uint64 const end_tick,
              double const time,
              double const duration) {
    auto const* const begin{as_of(series, begin_tick)};
    auto const* const end{as_of(series, end_tick)};
    if (!begin || !end || *end < *begin) {
        return;
    }
    builder.add(
        time, (static_cast<double>(*end) - static_cast<double>(*begin)) / duration, duration);
}
} // namespace telemetry_dashboard

auto telemetry_metric_title(ETelemetryDashboardMetric const metric) -> FString {
    switch (metric) {
        case ETelemetryDashboardMetric::RequestedTimeScaleRatio:
            return TEXT("Observed / requested time scale");
        case ETelemetryDashboardMetric::ObservedTimeScale:
            return TEXT("Observed time scale");
        case ETelemetryDashboardMetric::TicksPerRealSecond:
            return TEXT("Ticks per real second");
        case ETelemetryDashboardMetric::RealSampleInterval:
            return TEXT("Real sample interval");
        case ETelemetryDashboardMetric::ActiveEntities:
            return TEXT("Active entities");
        case ETelemetryDashboardMetric::PlayerShips:
            return TEXT("Active player ships");
        case ETelemetryDashboardMetric::Turrets:
            return TEXT("Active turrets");
        case ETelemetryDashboardMetric::CapitalShips:
            return TEXT("Active capital ships");
        case ETelemetryDashboardMetric::CapitalShipFighters:
            return TEXT("Active capital-ship fighters");
        case ETelemetryDashboardMetric::TubeSpinners:
            return TEXT("Active tube spinners");
        case ETelemetryDashboardMetric::ActiveLasers:
            return TEXT("Active lasers");
        case ETelemetryDashboardMetric::RegistrySlots:
            return TEXT("Registry slots");
        case ETelemetryDashboardMetric::OccupiedSpatialCells:
            return TEXT("Occupied spatial cells");
        case ETelemetryDashboardMetric::SpawnRate:
            return TEXT("Spawn rate");
        case ETelemetryDashboardMetric::DestructionRate:
            return TEXT("Destruction rate");
        case ETelemetryDashboardMetric::KillRate:
            return TEXT("Kill rate");
        case ETelemetryDashboardMetric::LaserFireRate:
            return TEXT("Laser-fire rate");
        case ETelemetryDashboardMetric::GridRebuildRate:
            return TEXT("Grid-rebuild rate");
        case ETelemetryDashboardMetric::RangeQueryRate:
            return TEXT("Range-query rate");
        case ETelemetryDashboardMetric::LineTraceRate:
            return TEXT("Line-trace rate");
        case ETelemetryDashboardMetric::SweepTraceRate:
            return TEXT("Sweep-trace rate");
        case ETelemetryDashboardMetric::COUNT:
            break;
    }
    return {};
}

auto telemetry_metric_units(ETelemetryDashboardMetric const metric) -> FString {
    switch (metric) {
        case ETelemetryDashboardMetric::RequestedTimeScaleRatio:
            return TEXT("%");
        case ETelemetryDashboardMetric::ObservedTimeScale:
            return TEXT("x");
        case ETelemetryDashboardMetric::TicksPerRealSecond:
            return TEXT("ticks/s");
        case ETelemetryDashboardMetric::RealSampleInterval:
            return TEXT("s");
        case ETelemetryDashboardMetric::SpawnRate:
        case ETelemetryDashboardMetric::DestructionRate:
        case ETelemetryDashboardMetric::KillRate:
        case ETelemetryDashboardMetric::LaserFireRate:
        case ETelemetryDashboardMetric::GridRebuildRate:
        case ETelemetryDashboardMetric::RangeQueryRate:
        case ETelemetryDashboardMetric::LineTraceRate:
        case ETelemetryDashboardMetric::SweepTraceRate:
            return TEXT("/s");
        case ETelemetryDashboardMetric::COUNT:
            break;
        default:
            return TEXT("count");
    }
    return {};
}

auto FTelemetryRunAnalysis::find_metric(ETelemetryDashboardMetric const metric) const
    -> FTelemetryMetricSeries const* {
    return metrics.FindByPredicate([metric](auto const& value) { return value.metric == metric; });
}

auto analyze_level_telemetry_run(FLevelTelemetryRunRecord const& record) -> FTelemetryRunAnalysis {
    using namespace telemetry_dashboard;
    FTelemetryRunAnalysis result;
    TArray<FMetricBuilder> builders;
    for (uint8 value{}; value < static_cast<uint8>(ETelemetryDashboardMetric::COUNT); ++value) {
        builders.Emplace(static_cast<ETelemetryDashboardMetric>(value));
    }
    auto builder = [&builders](ETelemetryDashboardMetric metric) -> FMetricBuilder& {
        return builders[static_cast<uint8>(metric)];
    };

    auto const& realtime{record.completed_ticks_by_real_time};
    auto const count{realtime.num()};
    constexpr int32 first_measured_sample_index{2};
    auto const measured_interval_count{FMath::Max(0, count - first_measured_sample_index)};
    result.throughput_real_elapsed_seconds.Reserve(measured_interval_count);
    result.observed_time_scale.Reserve(measured_interval_count);
    result.requested_time_scale.Reserve(measured_interval_count);
    for (int32 index{first_measured_sample_index}; index < count; ++index) {
        auto const begin_time{realtime.time_at(index - 1)};
        auto const end_time{realtime.time_at(index)};
        auto const real_delta{end_time - begin_time};
        auto const begin_tick{realtime.value_at(index - 1)};
        auto const end_tick{realtime.value_at(index)};
        if (!FMath::IsFinite(real_delta) || real_delta <= 0.0 || end_tick < begin_tick) {
            continue;
        }
        auto const tick_delta{end_tick - begin_tick};
        auto const ticks_per_second{static_cast<double>(tick_delta) / real_delta};
        auto const observed{ticks_per_second * record.metadata.tick_period_seconds};
        if (FMath::IsFinite(observed)) {
            result.throughput_real_elapsed_seconds.Add(static_cast<float>(end_time));
            result.observed_time_scale.Add(static_cast<float>(observed));
            auto const* requested{as_of(record.tick_series.requested_time_scale, end_tick)};
            result.requested_time_scale.Add(static_cast<float>(
                requested ? *requested : record.metadata.initial_requested_time_scale));
            builder(ETelemetryDashboardMetric::ObservedTimeScale)
                .add(end_time, observed, real_delta);
            builder(ETelemetryDashboardMetric::TicksPerRealSecond)
                .add(end_time, ticks_per_second, real_delta);
            builder(ETelemetryDashboardMetric::RealSampleInterval)
                .add(end_time, real_delta, real_delta);
            double stable_requested{};
            if (requested_unchanged(record.tick_series.requested_time_scale,
                                    begin_tick,
                                    end_tick,
                                    stable_requested)) {
                builder(ETelemetryDashboardMetric::RequestedTimeScaleRatio)
                    .add(end_time, observed / stable_requested * 100.0, real_delta);
            }
        }

        add_gauge(builder(ETelemetryDashboardMetric::ActiveEntities),
                  record.tick_series.active_entities,
                  end_tick,
                  end_time,
                  real_delta);
        constexpr ETelemetryDashboardMetric type_metrics[]{
            ETelemetryDashboardMetric::PlayerShips,
            ETelemetryDashboardMetric::Turrets,
            ETelemetryDashboardMetric::CapitalShips,
            ETelemetryDashboardMetric::CapitalShipFighters,
            ETelemetryDashboardMetric::TubeSpinners};
        for (int32 type{}; type < FLevelTelemetryTickSeries::entity_type_count; ++type) {
            add_gauge(builder(type_metrics[type]),
                      record.tick_series.active_entities_by_type[type],
                      end_tick,
                      end_time,
                      real_delta);
        }
        add_gauge(builder(ETelemetryDashboardMetric::ActiveLasers),
                  record.tick_series.active_lasers,
                  end_tick,
                  end_time,
                  real_delta);
        add_gauge(builder(ETelemetryDashboardMetric::RegistrySlots),
                  record.tick_series.registry_slot_count,
                  end_tick,
                  end_time,
                  real_delta);
        add_gauge(builder(ETelemetryDashboardMetric::OccupiedSpatialCells),
                  record.tick_series.occupied_spatial_cell_count,
                  end_tick,
                  end_time,
                  real_delta);
        add_rate(builder(ETelemetryDashboardMetric::SpawnRate),
                 record.tick_series.spawned_entities,
                 begin_tick,
                 end_tick,
                 end_time,
                 real_delta);
        add_rate(builder(ETelemetryDashboardMetric::DestructionRate),
                 record.tick_series.destroyed_entities,
                 begin_tick,
                 end_tick,
                 end_time,
                 real_delta);
        add_rate(builder(ETelemetryDashboardMetric::KillRate),
                 record.tick_series.kills,
                 begin_tick,
                 end_tick,
                 end_time,
                 real_delta);
        add_rate(builder(ETelemetryDashboardMetric::LaserFireRate),
                 record.tick_series.lasers_fired,
                 begin_tick,
                 end_tick,
                 end_time,
                 real_delta);
        add_rate(builder(ETelemetryDashboardMetric::GridRebuildRate),
                 record.tick_series.grid_rebuild_count,
                 begin_tick,
                 end_tick,
                 end_time,
                 real_delta);
        add_rate(builder(ETelemetryDashboardMetric::RangeQueryRate),
                 record.tick_series.range_query_count,
                 begin_tick,
                 end_tick,
                 end_time,
                 real_delta);
        add_rate(builder(ETelemetryDashboardMetric::LineTraceRate),
                 record.tick_series.line_trace_count,
                 begin_tick,
                 end_tick,
                 end_time,
                 real_delta);
        add_rate(builder(ETelemetryDashboardMetric::SweepTraceRate),
                 record.tick_series.sweep_trace_count,
                 begin_tick,
                 end_tick,
                 end_time,
                 real_delta);
    }
    result.metrics.Reserve(builders.Num());
    for (auto& value : builders) {
        result.metrics.Add(value.finish());
    }
    return result;
}

auto FTelemetryRunSummary::level_label() const -> FString {
    if (!level_display_name.IsEmpty()) {
        return level_display_name;
    }
    if (!level_id.IsNone()) {
        return level_id.ToString();
    }
    return map_name;
}

FTelemetryRunCatalog::FTelemetryRunCatalog(FString directory)
    : directory_{directory.IsEmpty() ? level_telemetry_runs_directory() : MoveTemp(directory)} {}

void FTelemetryRunCatalog::refresh() {
    auto const previous_selection{selected_run_id_};
    all_.Reset();
    unreadable_file_count_ = 0;
    directory_exists_ = IFileManager::Get().DirectoryExists(*directory_);
    if (directory_exists_) {
        TArray<FString> files;
        IFileManager::Get().FindFiles(
            files, *FPaths::Combine(directory_, TEXT("*.json")), true, false);
        for (auto const& filename : files) {
            auto const path{FPaths::Combine(directory_, filename)};
            auto summary{telemetry_dashboard::read_summary(path)};
            if (!summary) {
                ++unreadable_file_count_;
                continue;
            }
            all_.Add(MoveTemp(*summary));
        }
    }
    Algo::Sort(all_, [](auto const& left, auto const& right) {
        return left.launched_utc > right.launched_utc;
    });
    level_filters_.Reset();
    level_filters_.Add(TEXT("All levels"));
    for (auto const& run : all_) {
        level_filters_.AddUnique(run.level_label());
    }
    if (!level_filters_.Contains(level_filter_)) {
        level_filter_ = TEXT("All levels");
    }
    selected_run_id_ = previous_selection;
    rebuild_filter();
}

void FTelemetryRunCatalog::set_level_filter(FString level_label) {
    level_filter_ =
        level_filters_.Contains(level_label) ? MoveTemp(level_label) : TEXT("All levels");
    rebuild_filter();
}

bool FTelemetryRunCatalog::select_run(FString const& run_id) {
    if (!filtered_.ContainsByPredicate(
            [&run_id](auto const& run) { return run.run_id == run_id; })) {
        return false;
    }
    selected_run_id_ = run_id;
    load_selection();
    return selected_record_.IsSet();
}

void FTelemetryRunCatalog::rebuild_filter() {
    filtered_.Reset();
    for (auto const& run : all_) {
        if (level_filter_ == TEXT("All levels") || run.level_label() == level_filter_) {
            filtered_.Add(run);
        }
    }
    if (!filtered_.ContainsByPredicate(
            [this](auto const& run) { return run.run_id == selected_run_id_; })) {
        selected_run_id_ = filtered_.IsEmpty() ? FString{} : filtered_[0].run_id;
    }
    load_selection();
}

void FTelemetryRunCatalog::load_selection() {
    selected_record_.Reset();
    selected_error_.Reset();
    auto const* summary{filtered_.FindByPredicate(
        [this](auto const& run) { return run.run_id == selected_run_id_; })};
    if (!summary) {
        return;
    }
    auto record{read_level_telemetry_run(summary->path)};
    if (!record) {
        selected_error_ = record.error();
        return;
    }
    selected_record_ = MoveTemp(*record);
}
