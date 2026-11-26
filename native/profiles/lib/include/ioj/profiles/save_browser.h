#pragma once

#include <ioj/profiles/profile_report.h>
#include <ioj/profiles/profile_summary.h>
#include <ioj/profiles/score_record.h>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ioj::profiles {
class ProfileManager;

class SaveGameBrowser {
  public:
    void refresh(ProfileManager const& manager);
    bool load_profile_report(ProfileManager const& manager, std::string const& profile_id);
    auto summaries() const -> std::span<ProfileSummary const>;
    auto active_profile_id() const -> std::string_view;
    auto loaded_profile_report() const -> ProfileReport const*;
  private:
    std::string active_profile_id_;
    std::vector<ProfileSummary> summaries_;
    std::optional<ProfileReport> loaded_report_{};
};

auto make_summary(std::string profile_id,
                  std::string display_name,
                  std::span<ScoreRecord const> records) -> ProfileSummary;
auto make_report(std::string profile_id, std::span<ScoreRecord const> records) -> ProfileReport;
}
