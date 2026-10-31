#pragma once

#include <ioj/levels/launch_options.h>
#include <ioj/sim/telemetry/report.h>

#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ioj::sim::telemetry {
struct RunStatistics {
    double simulated_seconds{};
    std::int32_t active_entities{};
    std::int32_t active_lasers{};
    std::int32_t spawned_entities{};
    std::int32_t destroyed_entities{};
    std::int32_t kills{};
    std::int32_t lasers_fired{};
    bool mission_complete{};
    std::uint64_t shots{};
    std::uint64_t hits{};
    double damage_dealt{};
    std::uint64_t combat_kills{};
    std::uint64_t losses{};
};

struct RunComparison {
    bool different_level_source{};
    bool different_settings_source{};
    bool different_presentation_mode{};
};

[[nodiscard]] auto summarize_run(LevelTelemetryReport const& report) -> RunStatistics;
[[nodiscard]] auto make_run_report(LevelTelemetryRunRecord record,
                                   levels::LaunchOptions const& options,
                                   bool presentation_enabled) -> LevelTelemetryReport;
[[nodiscard]] auto compare_run_context(LevelTelemetryReport const& baseline,
                                       LevelTelemetryReport const& candidate) -> RunComparison;

struct RunSummary {
    std::filesystem::path path{};
    std::string run_id{};
    std::string map_name{};
    std::string level_id{};
    std::string level_display_name{};
    std::string launched_utc{};
    LevelTelemetryRunEndReason completion_reason{LevelTelemetryRunEndReason::WorldEnd};

    [[nodiscard]] auto level_label() const -> std::string_view;
};

auto read_run_report(std::filesystem::path const& path)
    -> std::expected<LevelTelemetryReport, std::string>;

class RunCatalog {
  public:
    explicit RunCatalog(std::filesystem::path directory);

    void refresh();
    void set_level_filter(std::string level_label);
    auto select_run(std::string_view run_id) -> bool;
    auto select_baseline(std::string_view run_id) -> bool;

    [[nodiscard]] auto get_runs() const -> std::span<RunSummary const> { return filtered_; }
    [[nodiscard]] auto get_level_filters() const -> std::span<std::string const> {
        return level_filters_;
    }
    [[nodiscard]] auto get_selected_run_id() const -> std::string const& {
        return selected_run_id_;
    }
    [[nodiscard]] auto get_selected_record() const -> LevelTelemetryReport const* {
        return selected_record_ ? &*selected_record_ : nullptr;
    }
    [[nodiscard]] auto get_selected_error() const -> std::string const& { return selected_error_; }
    [[nodiscard]] auto get_baseline_run_id() const -> std::string const& {
        return baseline_run_id_;
    }
    [[nodiscard]] auto get_baseline_record() const -> LevelTelemetryReport const* {
        return baseline_record_ ? &*baseline_record_ : nullptr;
    }
    [[nodiscard]] auto get_baseline_candidates() const -> std::vector<RunSummary>;
    [[nodiscard]] auto get_unreadable_file_count() const noexcept -> std::size_t {
        return unreadable_file_count_;
    }
    [[nodiscard]] auto get_level_filter() const -> std::string const& { return level_filter_; }
  private:
    void rebuild_filter();
    void load_selection();

    std::filesystem::path directory_;
    std::vector<RunSummary> all_;
    std::vector<RunSummary> filtered_;
    std::vector<std::string> level_filters_;
    std::string level_filter_{"All levels"};
    std::string selected_run_id_;
    std::optional<LevelTelemetryReport> selected_record_;
    std::string baseline_run_id_;
    std::optional<LevelTelemetryReport> baseline_record_;
    std::string selected_error_;
    std::size_t unreadable_file_count_{};
};
}
