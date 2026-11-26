#include <ioj/profiles/save_browser.h>

#include <ioj/display_name.h>
#include <ioj/profiles/profile_manager.h>

#include <algorithm>
#include <format>
#include <utility>
#include <vector>

namespace ioj::profiles {
void SaveGameBrowser::refresh(ProfileManager const& manager) {
    loaded_report_.reset();
    active_profile_id_ = manager.active_profile_id();
    summaries_.clear();

    for (auto const& profile : manager.profiles()) {
        summaries_.push_back(
            {.profile_id = profile.profile_id,
             .display_name = profile.display_name,
             .created_at = profile.created_at,
             .last_played_at = profile.last_played_at,
             .total_simulation_duration_seconds = profile.total_simulation_duration_seconds,
             .total_kills = profile.total_kills,
             .outcome_count = profile.outcome_count});
    }

    std::ranges::sort(summaries_, [](auto const& lhs, auto const& rhs) {
        if (lhs.last_played_at != rhs.last_played_at) {
            return lhs.last_played_at > rhs.last_played_at;
        }
        return lhs.profile_id < rhs.profile_id;
    });
}
bool SaveGameBrowser::load_profile_report(ProfileManager const& manager,
                                          std::string const& profile_id) {
    if (loaded_report_ && loaded_report_->profile_id == profile_id) {
        return true;
    }

    std::vector<ScoreRecord> records;
    if (!manager.load_profile_records(profile_id, records)) {
        loaded_report_.reset();
        return false;
    }

    loaded_report_ = make_report(profile_id, records);
    return true;
}
auto SaveGameBrowser::summaries() const -> std::span<ProfileSummary const> {
    return summaries_;
}

auto SaveGameBrowser::active_profile_id() const -> std::string_view {
    return active_profile_id_;
}
auto SaveGameBrowser::loaded_profile_report() const -> ProfileReport const* {
    return loaded_report_ ? &*loaded_report_ : nullptr;
}
auto make_summary(std::string profile_id,
                  std::string display_name,
                  std::span<ScoreRecord const> records) -> ProfileSummary {
    ProfileSummary summary{.profile_id = std::move(profile_id),
                           .display_name = std::move(display_name),
                           .outcome_count = static_cast<std::int32_t>(records.size())};
    if (!records.empty()) {
        summary.created_at = records.front().date;
        summary.last_played_at = records.front().date;
        for (auto const& record : records) {
            summary.created_at = std::min(summary.created_at, record.date);
            summary.last_played_at = std::max(summary.last_played_at, record.date);
            summary.total_simulation_duration_seconds += record.time_seconds;
            summary.total_kills += record.kills;
        }
    }
    return summary;
}
auto make_report(std::string profile_id, std::span<ScoreRecord const> records) -> ProfileReport {
    ProfileReport report{.profile_id = std::move(profile_id)};
    auto const count{records.size()};
    report.outcomes.reserve(count);
    for (std::size_t index{}; index < count; ++index) {
        auto const& record{records[index]};
        report.outcomes.push_back({
            .outcome_id = std::format("{}_{}", record.level_name, index),
            .display_name = format_level_display_name(record.level_name),
            .completed_at = record.date,
            .simulation_duration_seconds = record.time_seconds,
            .kills = record.kills,
            .result = std::string{to_serialized_string(record.outcome)},
            .statistics = {{"End reason", std::string{to_serialized_string(record.end_reason)}}},
        });
    }
    return report;
}
}
