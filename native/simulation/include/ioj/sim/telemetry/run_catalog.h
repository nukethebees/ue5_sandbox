#pragma once

#include <ioj/sim/telemetry/report.h>
#include <ioj/sim/telemetry/run_summary.h>

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ioj::sim::telemetry {
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
    [[nodiscard]] auto get_selected_record() const -> Report const* {
        return selected_record_ ? &*selected_record_ : nullptr;
    }
    [[nodiscard]] auto get_selected_error() const -> std::string const& { return selected_error_; }
    [[nodiscard]] auto get_baseline_run_id() const -> std::string const& {
        return baseline_run_id_;
    }
    [[nodiscard]] auto get_baseline_record() const -> Report const* {
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
    std::optional<Report> selected_record_;
    std::string baseline_run_id_;
    std::optional<Report> baseline_record_;
    std::string selected_error_;
    std::size_t unreadable_file_count_{};
};
}
