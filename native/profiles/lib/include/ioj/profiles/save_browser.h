#pragma once

#include <ioj/profiles/profile_listing.h>
#include <ioj/profiles/profile_report.h>
#include <ioj/profiles/profile_summary.h>
#include <ioj/profiles/score_record.h>

#include <functional>
#include <optional>
#include <span>
#include <string_view>

namespace ioj::profiles {
class ProfileManager;

class SaveGameBrowser {
  public:
    using SummarySource = std::function<ProfileListing()>;
    using ReportSource = std::function<std::optional<ProfileReport>(std::string const&)>;

    SaveGameBrowser() = default;
    SaveGameBrowser(SummarySource summary_source, ReportSource report_source);

    void refresh();
    bool load_profile_report(std::string const& profile_id);
    auto get_summaries() const -> std::span<ProfileSummary const>;
    auto get_active_profile_id() const -> std::string_view;
    auto get_loaded_profile_report() const -> ProfileReport const*;
  private:
    SummarySource summary_source_{};
    ReportSource report_source_{};
    ProfileListing listing_{};
    std::optional<ProfileReport> loaded_report_{};
};

auto make_summary(std::string profile_id,
                  std::string display_name,
                  std::span<ScoreRecord const> records) -> ProfileSummary;
auto make_report(std::string profile_id, std::span<ScoreRecord const> records) -> ProfileReport;
auto discover_profiles(ProfileManager const& manager) -> ProfileListing;
auto load_report(ProfileManager const& manager, std::string const& profile_id)
    -> std::optional<ProfileReport>;
}
