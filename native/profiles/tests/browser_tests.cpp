#include "test_support.h"
#include <ioj/profiles/profile_manager.h>
#include <ioj/profiles/save_browser.h>

#include <gtest/gtest.h>

#include <vector>

namespace ioj::profiles {

TEST(SaveGameBrowser, RefreshReplacesCachedSummaries) {
    FakeProfileStorage storage;
    auto manager{ProfileManager::open(storage.make())};
    ASSERT_TRUE(manager);
    SaveGameBrowser browser;
    browser.refresh(*manager);
    ASSERT_EQ(browser.summaries().size(), 1u);
    EXPECT_EQ(browser.active_profile_id(), manager->active_profile_id());

    auto const created{manager->create_profile("Second")};
    ASSERT_EQ(created.result, CreateProfileResult::succeeded);
    ASSERT_TRUE(manager->append_score_record({.date = 1, .kills = 4}));
    browser.refresh(*manager);
    ASSERT_EQ(browser.summaries().size(), 2u);
    EXPECT_EQ(browser.active_profile_id(), created.profile_id);
    EXPECT_EQ(browser.summaries().front().total_kills, 4u);
}

TEST(SaveGameBrowser, OrderIsNewestFirstAndDeterministic) {
    FakeProfileStorage storage{
        .index_exists = true,
        .index = {.active_profile_id = "older",
                  .profiles = {{.profile_id = "zulu", .last_played_at = 2},
                               {.profile_id = "older", .last_played_at = 1},
                               {.profile_id = "alpha", .last_played_at = 2}}},
        .results = {{"older", ProfileResultsData{.score_records = {{.date = 1}}}}}};
    auto manager{ProfileManager::open(storage.make())};
    ASSERT_TRUE(manager);

    SaveGameBrowser browser;
    browser.refresh(*manager);
    auto const summaries{browser.summaries()};
    ASSERT_EQ(summaries.size(), 3u);
    EXPECT_EQ(summaries[0].profile_id, "alpha");
    EXPECT_EQ(summaries[1].profile_id, "zulu");
    EXPECT_EQ(summaries[2].profile_id, "older");
}

TEST(SaveGameBrowser, ReportsAreLoadedLazilyAndRefreshInvalidatesTheCache) {
    FakeProfileStorage storage;
    auto manager{ProfileManager::open(storage.make())};
    ASSERT_TRUE(manager);
    auto const first_profile{manager->active_profile_id()};
    ASSERT_TRUE(manager->append_score_record({.date = 1, .level_name = "mission"}));
    auto const second{manager->create_profile("Second")};
    ASSERT_EQ(second.result, CreateProfileResult::succeeded);

    SaveGameBrowser browser;
    browser.refresh(*manager);
    EXPECT_EQ(browser.loaded_profile_report(), nullptr);
    ASSERT_TRUE(browser.load_profile_report(*manager, first_profile));
    EXPECT_EQ(browser.loaded_profile_report()->outcomes.size(), 1u);
    EXPECT_EQ(manager->active_profile_id(), second.profile_id);

    storage.results.at(first_profile).score_records.clear();
    ASSERT_TRUE(browser.load_profile_report(*manager, first_profile));
    EXPECT_EQ(browser.loaded_profile_report()->outcomes.size(), 1u);

    browser.refresh(*manager);
    EXPECT_EQ(browser.loaded_profile_report(), nullptr);
    ASSERT_TRUE(browser.load_profile_report(*manager, first_profile));
    EXPECT_TRUE(browser.loaded_profile_report()->outcomes.empty());
    EXPECT_FALSE(browser.load_profile_report(*manager, "missing"));
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

TEST(ProfileAdapter, AdaptsTypedLevelResultsIntoReportOutcomes) {

    std::vector<ScoreRecord> const records{{.date = test_date(2026, 4, 3, 12, 30),
                                            .level_name = "test_level_42",
                                            .outcome = LevelOutcome::Defeat,
                                            .end_reason = sim::RunEndReason::DurationReached,
                                            .kills = 8,
                                            .time_seconds = 91.f}};

    auto const report{make_report("profile", records)};

    EXPECT_EQ(report.outcomes.size(), 1) << "Report has one outcome";
    auto const& outcome{report.outcomes[0]};
    EXPECT_EQ(outcome.display_name, std::string{"Test Level 42"})
        << "Level name is formatted for display";
    EXPECT_EQ(outcome.kills, 8) << "Kills remain typed";
    EXPECT_EQ(outcome.result, std::string{"defeat"}) << "End state is displayed";
    EXPECT_EQ(outcome.statistics.size(), 1) << "Terminal reason becomes a display statistic";
    EXPECT_EQ(outcome.statistics.back().value, std::string{"duration_reached"})
        << "Failure reason is included";
}

}
