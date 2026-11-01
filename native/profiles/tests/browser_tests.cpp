#include "test_support.h"
#include <ioj/profiles/file_profile_storage.h>
#include <ioj/profiles/progression.h>
#include <ioj/profiles/save_browser.h>

#include <gtest/gtest.h>

#include <chrono>
#include <unordered_map>

namespace ioj::profiles {

namespace save_game_browser_test {
auto make_summary(std::string profile_id, Timestamp last_played_at, std::int32_t const total_kills)
    -> ProfileSummary {
    return {
        .profile_id = std::move(profile_id),
        .display_name = "Display name",
        .created_at = test_date(2026, 1, 1),
        .last_played_at = last_played_at,
        .total_simulation_duration_seconds = 42.f,
        .total_kills = total_kills,
        .outcome_count = 1,
    };
}

auto no_report(std::string const&) -> std::optional<ProfileReport> {
    return {};
}
}

TEST(SaveGameBrowser, RefreshReplacesCachedSummaries) {

    std::int32_t refresh_count{};
    SaveGameBrowser browser{
        [&refresh_count] {
            ++refresh_count;
            if (refresh_count == 1) {
                return ProfileListing{.active_profile_id = "first",
                                      .summaries = {save_game_browser_test::make_summary(
                                          "first", test_date(2026, 1, 1), 10)}};
            }

            if (refresh_count == 2) {
                return ProfileListing{.active_profile_id = "second",
                                      .summaries = {save_game_browser_test::make_summary(
                                          "second", test_date(2026, 1, 2), 20)}};
            }
            return ProfileListing{};
        },
        save_game_browser_test::no_report};

    browser.refresh();
    ASSERT_EQ(browser.summaries().size(), 1) << "First refresh has one summary";
    EXPECT_EQ(browser.active_profile_id(), "first");
    EXPECT_EQ(browser.summaries()[0].profile_id, std::string{"first"})
        << "First refresh uses first result";

    browser.refresh();
    ASSERT_EQ(browser.summaries().size(), 1) << "Second refresh still has one summary";
    EXPECT_EQ(browser.active_profile_id(), "second");
    EXPECT_EQ(browser.summaries()[0].profile_id, std::string{"second"})
        << "Second refresh replaces first result";
    browser.refresh();
    EXPECT_TRUE(browser.summaries().empty()) << "Empty discovery clears stale summaries";
    EXPECT_TRUE(browser.active_profile_id().empty());
}

TEST(SaveGameBrowser, OrderIsNewestFirstAndDeterministic) {

    auto const shared_timestamp{test_date(2026, 6, 2)};
    SaveGameBrowser browser{
        [shared_timestamp] {
            return ProfileListing{
                .summaries = {
                    save_game_browser_test::make_summary("zulu", shared_timestamp, 1),
                    save_game_browser_test::make_summary("older", test_date(2026, 6, 1), 2),
                    save_game_browser_test::make_summary("alpha", shared_timestamp, 3),
                }};
        },
        save_game_browser_test::no_report};

    browser.refresh();

    auto const summaries{browser.summaries()};
    ASSERT_EQ(summaries.size(), 3);
    EXPECT_EQ(summaries[0].profile_id, std::string{"alpha"})
        << "Alphabetical tie-breaker comes first";
    EXPECT_EQ(summaries[1].profile_id, std::string{"zulu"}) << "Second tied profile follows";
    EXPECT_EQ(summaries[2].profile_id, std::string{"older"}) << "Older profile comes last";
}

TEST(SaveGameBrowser, ReportsAreLoadedLazilyAndRefreshInvalidatesTheCache) {

    std::int32_t report_load_count{};
    SaveGameBrowser browser{
        [] {
            return ProfileListing{.summaries = {save_game_browser_test::make_summary(
                                      "profile", test_date(2026, 1, 1), 5)}};
        },
        [&report_load_count](std::string const& profile_id) -> std::optional<ProfileReport> {
            ++report_load_count;
            if (profile_id != "profile") {
                return {};
            }
            return ProfileReport{
                .profile_id = profile_id,
                .outcomes = {{.outcome_id = "outcome"}},
            };
        }};

    browser.refresh();
    EXPECT_TRUE(report_load_count == 0) << "Refresh does not load a report";
    EXPECT_TRUE(browser.loaded_profile_report() == nullptr) << "No report is initially cached";

    ASSERT_TRUE(browser.load_profile_report("profile")) << "Known profile report loads";
    EXPECT_TRUE(report_load_count == 1) << "Report source is called once";
    EXPECT_EQ(browser.loaded_profile_report()->outcomes.size(), 1)
        << "Loaded report retains its outcome";

    EXPECT_TRUE(browser.load_profile_report("profile")) << "Reloading selected profile succeeds";
    EXPECT_TRUE(report_load_count == 1) << "Selected report is served from cache";

    browser.refresh();
    EXPECT_TRUE(browser.loaded_profile_report() == nullptr)
        << "Refresh invalidates the selected report";
    ASSERT_TRUE(browser.load_profile_report("profile"));
    EXPECT_EQ(report_load_count, 2);
    EXPECT_FALSE(browser.load_profile_report("missing"));
    EXPECT_EQ(browser.loaded_profile_report(), nullptr);
}

TEST(ProfileAdapter, BuildsLightweightProfileSummary) {

    std::vector<ScoreRecord> const records{
        {.date = test_date(2026, 4, 3), .kills = 7, .time_seconds = 30.f},
        {.date = test_date(2026, 4, 1), .kills = 5, .time_seconds = 12.5f},
    };

    auto const summary{make_summary("profile", "Pilot", records)};

    EXPECT_EQ(summary.profile_id, std::string{"profile"}) << "Profile ID remains intact";
    EXPECT_EQ(summary.display_name, std::string{"Pilot"}) << "Display name remains intact";
    EXPECT_TRUE(summary.created_at == test_date(2026, 4, 1)) << "Created date uses earliest result";
    EXPECT_TRUE(summary.last_played_at == test_date(2026, 4, 3))
        << "Last played date uses latest result";
    EXPECT_TRUE(summary.total_simulation_duration_seconds == 42.5f) << "Durations are accumulated";
    EXPECT_EQ(summary.total_kills, 12) << "Kills are accumulated";
    EXPECT_EQ(summary.outcome_count, 2) << "Outcome count is retained";
}

TEST(ProfileAdapter, AdaptsTypedMissionResultsIntoReportOutcomes) {

    std::vector<ScoreRecord> const records{{.date = test_date(2026, 4, 3, 12, 30),
                                            .level_name = "test_level_42",
                                            .outcome = MissionOutcome::Defeat,
                                            .objective_id = "deadline",
                                            .objective_outcome = ObjectiveOutcome::Expired,
                                            .kills = 8,
                                            .time_seconds = 91.f}};

    auto const report{make_report("profile", records)};

    EXPECT_EQ(report.outcomes.size(), 1) << "Report has one outcome";
    auto const& outcome{report.outcomes[0]};
    EXPECT_EQ(outcome.display_name, std::string{"Test Level 42"})
        << "Level name is formatted for display";
    EXPECT_EQ(outcome.kills, 8) << "Kills remain typed";
    EXPECT_EQ(outcome.result, std::string{"defeat"}) << "End state is displayed";
    EXPECT_EQ(outcome.statistics.size(), 2) << "Mission details become display statistics";
    EXPECT_EQ(outcome.statistics.back().value, std::string{"expired"})
        << "Failure reason is included";
}

}
