#pragma once

#include <ioj/persistence/profile_manager.h>

#include <optional>

namespace ioj::persistence {
struct LevelOutcomeStatistic {
    std::string label{};
    std::string value{};
};
struct LevelOutcomeSummary {
    std::string outcome_id{};
    std::string display_name{};
    Timestamp completed_at{};
    float simulation_duration_seconds{};
    std::int32_t kills{};
    std::string result{};
    std::vector<LevelOutcomeStatistic> statistics{};
};
struct ProfileSummary {
    std::string profile_id{};
    std::string display_name{};
    Timestamp created_at{};
    Timestamp last_played_at{};
    float total_simulation_duration_seconds{};
    std::int32_t total_kills{};
    std::int32_t outcome_count{};
    bool active{};
};
struct ProfileReport {
    std::string profile_id{};
    std::vector<LevelOutcomeSummary> outcomes{};
};

class SaveGameBrowser {
  public:
    using SummarySource = std::function<std::vector<ProfileSummary>()>;
    using ReportSource = std::function<std::optional<ProfileReport>(std::string const&)>;

    SaveGameBrowser() = default;
    SaveGameBrowser(SummarySource summary_source, ReportSource report_source);

    void refresh();
    bool load_profile_report(std::string const& profile_id);
    auto get_summaries() const -> std::span<ProfileSummary const>;
    auto get_loaded_profile_report() const -> ProfileReport const*;
  private:
    SummarySource summary_source_{};
    ReportSource report_source_{};
    std::vector<ProfileSummary> summaries_{};
    std::optional<ProfileReport> loaded_report_{};
};

auto make_summary(std::string profile_id,
                  std::string display_name,
                  std::span<ScoreRecord const> records) -> ProfileSummary;
auto make_report(std::string profile_id, std::span<ScoreRecord const> records) -> ProfileReport;
auto discover_profiles(ProfileManager const& manager) -> std::vector<ProfileSummary>;
auto load_report(ProfileManager const& manager, std::string const& profile_id)
    -> std::optional<ProfileReport>;
}
