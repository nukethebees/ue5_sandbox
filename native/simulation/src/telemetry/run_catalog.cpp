#include <ioj/sim/telemetry/run_catalog.h>

#include <ioj/sim/telemetry/report_json.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <type_traits>
#include <utility>

namespace ioj::sim::telemetry {
auto make_run_report(LevelTelemetryRunRecord record,
                     levels::LaunchOptions const& options,
                     bool const presentation_enabled) -> LevelTelemetryReport {
    LevelTelemetryReport report{std::move(record)};
    report.started_paused = options.launch_mode == levels::LaunchMode::Paused;
    report.opens_telemetry = options.results_navigation == levels::ResultsNavigation::Telemetry;
    report.presentation_enabled = presentation_enabled;
    return report;
}
auto summarize_run(LevelTelemetryReport const& report) -> RunStatistics {
    auto const last{[](auto const& series) { return series.is_empty() ? 0 : series.last_value(); }};
    auto const& run{report.run};
    RunStatistics result{
        .simulated_seconds =
            static_cast<double>(run.completion.completed_ticks.value()) / run.tick_rate_hz.get_hz(),
        .active_entities = last(run.tick_series.active_entities),
        .active_lasers = last(run.tick_series.active_lasers),
        .spawned_entities = last(run.tick_series.spawned_entities),
        .destroyed_entities = last(run.tick_series.destroyed_entities),
        .kills = last(run.tick_series.kills),
        .lasers_fired = last(run.tick_series.lasers_fired),
        .mission_complete = run.completion.reason == LevelTelemetryRunEndReason::MissionSucceeded ||
                            run.completion.reason == LevelTelemetryRunEndReason::MissionFailed};
    if (!run.battle_samples.empty()) {
        auto const sum{[](auto const& counts) {
            using Value = std::remove_cvref_t<decltype(counts[0][0])>;
            Value total{};
            for (auto const& row : counts) {
                for (auto const value : row) {
                    total += value;
                }
            }
            return total;
        }};
        auto const& combat{run.battle_samples.back().combat};
        result.shots = sum(combat.shots);
        result.hits = sum(combat.hits);
        result.damage_dealt = sum(combat.damage_dealt);
        result.combat_kills = sum(combat.kills);
        result.losses = sum(combat.losses);
    }
    return result;
}
auto compare_run_context(LevelTelemetryReport const& baseline,
                         LevelTelemetryReport const& candidate) -> RunComparison {
    return {.different_level_source =
                baseline.run.metadata.level_source != candidate.run.metadata.level_source,
            .different_settings_source =
                baseline.run.metadata.settings_source != candidate.run.metadata.settings_source,
            .different_presentation_mode =
                baseline.presentation_enabled != candidate.presentation_enabled};
}
namespace {
auto read_text(std::filesystem::path const& path) -> std::expected<std::string, std::string> {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return std::unexpected{"Could not read telemetry file '" + path.string() + "'"};
    }
    std::string text{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
    if (stream.bad()) {
        return std::unexpected{"Could not read telemetry file '" + path.string() + "'"};
    }
    return text;
}

auto read_summary(std::filesystem::path const& path) -> std::expected<RunSummary, std::string> {
    auto const text{read_text(path)};
    if (!text) {
        return std::unexpected{text.error()};
    }
    try {
        auto const root = nlohmann::json::parse(*text);
        if (root.at("schema_version") != LevelTelemetryRunRecord::schema_version) {
            return std::unexpected{"Unsupported telemetry schema"};
        }
        auto const& level{root.at("level")};
        auto const reason{try_parse_serialized_level_telemetry_run_end_reason(
            root.at("completion").at("reason").get<std::string>())};
        if (!reason) {
            return std::unexpected{"Unknown completion reason"};
        }
        return RunSummary{.path = path,
                          .run_id = root.at("run_id").get<std::string>(),
                          .map_name = level.at("map_name").get<std::string>(),
                          .level_id = level.at("level_id").get<std::string>(),
                          .level_display_name = level.at("display_name").get<std::string>(),
                          .launched_utc =
                              root.at("timestamps").at("launched_utc").get<std::string>(),
                          .completion_reason = *reason};
    } catch (nlohmann::json::exception const& error) {
        return std::unexpected{std::string{error.what()}};
    }
}
}

auto read_run_report(std::filesystem::path const& path)
    -> std::expected<LevelTelemetryReport, std::string> {
    auto text{read_text(path)};
    if (!text) {
        return std::unexpected{std::move(text.error())};
    }
    auto report{deserialize_level_telemetry_report(*text)};
    if (!report) {
        return std::unexpected{path.string() + ": " + report.error()};
    }
    return report;
}

auto RunSummary::level_label() const -> std::string_view {
    if (!level_display_name.empty()) {
        return level_display_name;
    }
    return level_id.empty() ? std::string_view{map_name} : std::string_view{level_id};
}

RunCatalog::RunCatalog(std::filesystem::path directory)
    : directory_{std::move(directory)} {}

void RunCatalog::refresh() {
    all_.clear();
    unreadable_file_count_ = 0;
    std::error_code error;
    std::filesystem::directory_iterator files{directory_, error};
    for (auto end{std::filesystem::directory_iterator{}}; !error && files != end;
         files.increment(error)) {
        auto const& entry{*files};
        if (!entry.is_regular_file(error) || entry.path().extension() != ".json") {
            continue;
        }
        auto summary{read_summary(entry.path())};
        if (!summary) {
            ++unreadable_file_count_;
            continue;
        }
        all_.push_back(std::move(*summary));
    }
    if (error && error != std::errc::no_such_file_or_directory) {
        ++unreadable_file_count_;
    }
    std::ranges::sort(all_, [](auto const& left, auto const& right) {
        if (left.launched_utc != right.launched_utc) {
            return left.launched_utc > right.launched_utc;
        }
        return left.run_id < right.run_id;
    });
    level_filters_ = {"All levels"};
    for (auto const& run : all_) {
        auto const label{run.level_label()};
        if (!std::ranges::contains(level_filters_, label)) {
            level_filters_.emplace_back(label);
        }
    }
    if (!std::ranges::contains(level_filters_, level_filter_)) {
        level_filter_ = "All levels";
    }
    rebuild_filter();
}

void RunCatalog::set_level_filter(std::string level_label) {
    level_filter_ =
        std::ranges::contains(level_filters_, level_label) ? std::move(level_label) : "All levels";
    rebuild_filter();
}

auto RunCatalog::select_run(std::string_view const run_id) -> bool {
    if (std::ranges::find(filtered_, run_id, &RunSummary::run_id) == filtered_.end()) {
        return false;
    }
    selected_run_id_ = run_id;
    load_selection();
    return selected_record_.has_value();
}

auto RunCatalog::select_baseline(std::string_view const run_id) -> bool {
    baseline_run_id_.clear();
    baseline_record_.reset();
    if (run_id.empty()) {
        return true;
    }
    for (auto const& run : get_baseline_candidates()) {
        if (run.run_id != run_id) {
            continue;
        }
        auto report{read_run_report(run.path)};
        if (!report) {
            return false;
        }
        baseline_run_id_ = run_id;
        baseline_record_ = std::move(*report);
        return true;
    }
    return false;
}

auto RunCatalog::get_baseline_candidates() const -> std::vector<RunSummary> {
    std::vector<RunSummary> result;
    if (selected_record_) {
        for (auto const& run : all_) {
            if (run.run_id != selected_run_id_ &&
                run.level_id == selected_record_->run.metadata.level_id) {
                result.push_back(run);
            }
        }
    }
    return result;
}

void RunCatalog::rebuild_filter() {
    filtered_.clear();
    for (auto const& run : all_) {
        if (level_filter_ == "All levels" || run.level_label() == level_filter_) {
            filtered_.push_back(run);
        }
    }
    if (std::ranges::find(filtered_, selected_run_id_, &RunSummary::run_id) == filtered_.end()) {
        selected_run_id_ = filtered_.empty() ? std::string{} : filtered_.front().run_id;
    }
    load_selection();
}

void RunCatalog::load_selection() {
    selected_record_.reset();
    selected_error_.clear();
    auto const found{std::ranges::find(filtered_, selected_run_id_, &RunSummary::run_id)};
    if (found == filtered_.end()) {
        baseline_run_id_.clear();
        baseline_record_.reset();
        return;
    }
    auto report{read_run_report(found->path)};
    if (!report) {
        selected_error_ = std::move(report.error());
        baseline_run_id_.clear();
        baseline_record_.reset();
        return;
    }
    selected_record_ = std::move(*report);
    auto const baseline{baseline_run_id_};
    (void)select_baseline(baseline);
}
}
