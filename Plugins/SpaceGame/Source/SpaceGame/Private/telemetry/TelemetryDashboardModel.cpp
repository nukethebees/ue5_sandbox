#include <SpaceGame/telemetry/TelemetryDashboardModel.h>

#include <SpaceGame/telemetry/LevelTelemetryJson.h>

#include <SandboxCore/container_ops.h>

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
        (schema != 1.0 && schema != 2.0 && schema != 3.0 &&
         schema != FLevelTelemetryReport::schema_version)) {
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
        series.simulated_elapsed_seconds.Add(static_cast<float>(time));
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

/* **************************************** */
// Metric analysis
/* **************************************** */
auto telemetry_metric_title(ETelemetryDashboardMetric const metric) -> FString {
    switch (metric) {
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
        case ETelemetryDashboardMetric::SpawnRate:
            return TEXT("Spawn rate");
        case ETelemetryDashboardMetric::DestructionRate:
            return TEXT("Destruction rate");
        case ETelemetryDashboardMetric::KillRate:
            return TEXT("Kill rate");
        case ETelemetryDashboardMetric::LaserFireRate:
            return TEXT("Laser-fire rate");
        case ETelemetryDashboardMetric::COUNT:
            break;
    }
    return {};
}

auto telemetry_metric_units(ETelemetryDashboardMetric const metric) -> FString {
    switch (metric) {
        case ETelemetryDashboardMetric::SpawnRate:
        case ETelemetryDashboardMetric::DestructionRate:
        case ETelemetryDashboardMetric::KillRate:
        case ETelemetryDashboardMetric::LaserFireRate:
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

auto analyze_level_telemetry_run(FLevelTelemetryReport const& record) -> FTelemetryRunAnalysis {
    using namespace telemetry_dashboard;
    FTelemetryRunAnalysis result;
    TArray<FMetricBuilder> builders;
    for (uint8 value{}; value < static_cast<uint8>(ETelemetryDashboardMetric::COUNT); ++value) {
        builders.Emplace(static_cast<ETelemetryDashboardMetric>(value));
    }

    auto builder = [&builders](ETelemetryDashboardMetric metric) -> FMetricBuilder& {
        return builders[static_cast<uint8>(metric)];
    };
    auto const use_battle_metrics{record.loaded_schema_version >= 2 &&
                                  record.battle_samples.Num() >= 2};

    if (!use_battle_metrics && record.metadata.tick_period_seconds > 0.0 &&
        FMath::IsFinite(record.metadata.tick_period_seconds)) {
        TArray<uint64> ticks;
        auto collect_ticks = [&ticks](auto const& series) {
            auto const count{series.num()};
            for (int32 index{}; index < count; ++index) {
                ticks.Add(series.time_at(index));
            }
        };
        collect_ticks(record.tick_series.active_entities);
        collect_ticks(record.tick_series.spawned_entities);
        collect_ticks(record.tick_series.destroyed_entities);
        collect_ticks(record.tick_series.kills);
        collect_ticks(record.tick_series.active_lasers);
        collect_ticks(record.tick_series.lasers_fired);
        for (auto const& series : record.tick_series.active_entities_by_type) {
            collect_ticks(series);
        }
        ticks.Add(record.completion.completed_ticks);
        ticks.Sort();

        constexpr ETelemetryDashboardMetric type_metrics[]{
            ETelemetryDashboardMetric::PlayerShips,
            ETelemetryDashboardMetric::Turrets,
            ETelemetryDashboardMetric::CapitalShips,
            ETelemetryDashboardMetric::CapitalShipFighters,
            ETelemetryDashboardMetric::TubeSpinners};
        auto const count{ticks.Num()};
        for (int32 index{1}; index < count; ++index) {
            auto const begin_tick{ticks[index - 1]};
            auto const end_tick{ticks[index]};
            if (end_tick == begin_tick) {
                continue;
            }
            auto const duration{static_cast<double>(end_tick - begin_tick) *
                                record.metadata.tick_period_seconds};
            auto const time{static_cast<double>(end_tick) * record.metadata.tick_period_seconds};
            add_gauge(builder(ETelemetryDashboardMetric::ActiveEntities),
                      record.tick_series.active_entities,
                      end_tick,
                      time,
                      duration);
            for (int32 type{}; type < ::ioj::sim::LevelTelemetryTickSeries::entity_type_count;
                 ++type) {
                add_gauge(builder(type_metrics[type]),
                          record.tick_series.active_entities_by_type[type],
                          end_tick,
                          time,
                          duration);
            }
            add_gauge(builder(ETelemetryDashboardMetric::ActiveLasers),
                      record.tick_series.active_lasers,
                      end_tick,
                      time,
                      duration);
            add_rate(builder(ETelemetryDashboardMetric::SpawnRate),
                     record.tick_series.spawned_entities,
                     begin_tick,
                     end_tick,
                     time,
                     duration);
            add_rate(builder(ETelemetryDashboardMetric::DestructionRate),
                     record.tick_series.destroyed_entities,
                     begin_tick,
                     end_tick,
                     time,
                     duration);
            add_rate(builder(ETelemetryDashboardMetric::KillRate),
                     record.tick_series.kills,
                     begin_tick,
                     end_tick,
                     time,
                     duration);
            add_rate(builder(ETelemetryDashboardMetric::LaserFireRate),
                     record.tick_series.lasers_fired,
                     begin_tick,
                     end_tick,
                     time,
                     duration);
        }
    }

    auto sum_counts = [](auto const& counts) {
        double total{};
        for (auto const& row : counts) {
            for (auto const value : row) {
                total += static_cast<double>(value);
            }
        }
        return static_cast<float>(total);
    };

    ml::reserve(record.battle_samples.Num(),
                result.battle_simulated_seconds,
                result.battle_alive_entities,
                result.battle_shots,
                result.battle_hits,
                result.battle_damage_dealt,
                result.battle_kills);
    for (auto const& sample : record.battle_samples) {
        result.battle_simulated_seconds.Add(static_cast<float>(sample.simulated_elapsed_seconds));
        result.battle_alive_entities.Add(sum_counts(sample.alive));
        result.battle_shots.Add(sum_counts(sample.combat.shots));
        result.battle_hits.Add(sum_counts(sample.combat.hits));
        result.battle_damage_dealt.Add(sum_counts(sample.combat.damage_dealt));
        result.battle_kills.Add(sum_counts(sample.combat.kills));
    }

    if (use_battle_metrics) {
        constexpr ETelemetryDashboardMetric type_metrics[]{
            ETelemetryDashboardMetric::PlayerShips,
            ETelemetryDashboardMetric::Turrets,
            ETelemetryDashboardMetric::CapitalShips,
            ETelemetryDashboardMetric::CapitalShipFighters,
            ETelemetryDashboardMetric::TubeSpinners,
        };

        auto add_rate = [&builder](ETelemetryDashboardMetric const metric,
                                   double const begin,
                                   double const end,
                                   double const time,
                                   double const duration) {
            if (end >= begin) {
                builder(metric).add(time, (end - begin) / duration, duration);
            }
        };

        auto type_total = [](::ioj::sim::telemetry::EntityCounts const& counts, int32 const type) {
            int32 total{};
            for (auto const& team : counts) {
                total += team[type];
            }
            return total;
        };

        auto const sample_count{record.battle_samples.Num()};
        for (int32 index{1}; index < sample_count; ++index) {
            auto const& begin{record.battle_samples[index - 1]};
            auto const& end{record.battle_samples[index]};
            auto const duration{end.simulated_elapsed_seconds - begin.simulated_elapsed_seconds};
            if (!FMath::IsFinite(duration) || duration <= 0.0) {
                continue;
            }

            auto const time{end.simulated_elapsed_seconds};
            builder(ETelemetryDashboardMetric::ActiveEntities)
                .add(time, sum_counts(end.alive), duration);
            for (int32 type{}; type < ::ioj::sim::LevelTelemetryTickSeries::entity_type_count;
                 ++type) {
                builder(type_metrics[type]).add(time, type_total(end.alive, type), duration);
            }

            builder(ETelemetryDashboardMetric::ActiveLasers).add(time, end.active_lasers, duration);
            add_rate(ETelemetryDashboardMetric::SpawnRate,
                     sum_counts(begin.combat.spawned),
                     sum_counts(end.combat.spawned),
                     time,
                     duration);
            add_rate(ETelemetryDashboardMetric::DestructionRate,
                     sum_counts(begin.combat.destroyed),
                     sum_counts(end.combat.destroyed),
                     time,
                     duration);
            add_rate(ETelemetryDashboardMetric::KillRate,
                     sum_counts(begin.combat.kills),
                     sum_counts(end.combat.kills),
                     time,
                     duration);
            add_rate(ETelemetryDashboardMetric::LaserFireRate,
                     begin.lasers_fired,
                     end.lasers_fired,
                     time,
                     duration);
        }
    }

    result.metrics.Reserve(builders.Num());
    for (auto& value : builders) {
        result.metrics.Add(value.finish());
    }

    return result;
}

/* **************************************** */
// Run catalog
/* **************************************** */
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

bool FTelemetryRunCatalog::select_baseline(FString const& run_id) {
    baseline_run_id_ = run_id;
    baseline_record_.Reset();
    if (run_id.IsEmpty()) {
        return true;
    }
    for (auto const& run : get_baseline_candidates()) {
        if (run.run_id != run_id) {
            continue;
        }
        auto record{read_level_telemetry_run(run.path)};
        if (!record) {
            return false;
        }
        baseline_record_ = MoveTemp(*record);
        return true;
    }
    return false;
}

auto FTelemetryRunCatalog::get_baseline_candidates() const -> TArray<FTelemetryRunSummary> {
    TArray<FTelemetryRunSummary> result;
    if (!selected_record_.IsSet()) {
        return result;
    }
    FName const selected_level{UTF8_TO_TCHAR(selected_record_->metadata.level_id.c_str())};
    for (auto const& run : all_) {
        if (run.run_id != selected_run_id_ && run.level_id == selected_level) {
            result.Add(run);
        }
    }
    return result;
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
    if (baseline_record_.IsSet() &&
        baseline_record_->metadata.level_id != selected_record_->metadata.level_id) {
        baseline_run_id_.Reset();
        baseline_record_.Reset();
    }
}
