#include <SpaceGame/telemetry/TelemetryDashboardModel.h>

#include <ioj/sim/telemetry/analysis.h>
#include <SpaceGame/telemetry/LevelTelemetryJson.h>

#include <SandboxCore/container_ops.h>

#include <Algo/Sort.h>
#include <Dom/JsonObject.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>
#include <Serialization/JsonSerializer.h>

#include <string_view>

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
        schema != ioj::sim::LevelTelemetryRunRecord::schema_version) {
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
    auto const utf8_reason{FTCHARToUTF8{*reason}};
    auto const parsed_reason{::ioj::sim::try_parse_serialized_level_telemetry_run_end_reason(
        std::string_view{utf8_reason.Get(), static_cast<std::size_t>(utf8_reason.Length())})};
    if (!parsed_reason.has_value()) {
        return std::unexpected{FString::Printf(TEXT("Unknown completion reason in '%s'"), *path)};
    }
    result.completion_reason = *parsed_reason;
    return result;
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

auto analyze_level_telemetry_run(ioj::sim::LevelTelemetryReport const& record)
    -> FTelemetryRunAnalysis {
    auto const native{::ioj::sim::telemetry::analyze({
        .tick_rate_hz = record.run.tick_rate_hz,
        .battle_samples = record.run.battle_samples,
    })};
    static_assert(static_cast<std::uint8_t>(::ioj::sim::telemetry::Metric::Count) ==
                  static_cast<std::uint8_t>(ETelemetryDashboardMetric::COUNT));

    FTelemetryRunAnalysis result;
    result.metrics.Reserve(static_cast<int32>(native.metrics.size()));
    for (auto const& metric : native.metrics) {
        FTelemetryMetricSeries series{
            .metric = static_cast<ETelemetryDashboardMetric>(metric.metric),
            .title = telemetry_metric_title(static_cast<ETelemetryDashboardMetric>(metric.metric)),
            .units = telemetry_metric_units(static_cast<ETelemetryDashboardMetric>(metric.metric)),
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
    FName const selected_level{UTF8_TO_TCHAR(selected_record_->run.metadata.level_id.c_str())};
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
        baseline_record_->run.metadata.level_id != selected_record_->run.metadata.level_id) {
        baseline_run_id_.Reset();
        baseline_record_.Reset();
    }
}
