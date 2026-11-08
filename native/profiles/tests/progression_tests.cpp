#include "test_support.h"
#include <ioj/profiles/file_profile_storage.h>
#include <ioj/profiles/progression.h>
#include <ioj/profiles/save_browser.h>

#include <gtest/gtest.h>

#include <chrono>
#include <unordered_map>

namespace ioj::profiles {

TEST(LevelProgress, SummarisesOnlyTheRequestedAuthoredLevel) {

    levels::LevelId const level_id{std::string{"border-skirmish"}};
    std::vector<ScoreRecord> const records{
        {.date = test_date(2026, 8, 20),
         .level_name = level_id.value,
         .outcome = LevelOutcome::Defeat,
         .kills = 12,
         .time_seconds = 30.0f},
        {.date = test_date(2026, 8, 21),
         .level_name = level_id.value,
         .outcome = LevelOutcome::Victory,
         .kills = 8,
         .time_seconds = 75.0f},
        {.date = test_date(2026, 8, 22),
         .level_name = level_id.value,
         .outcome = LevelOutcome::Victory,
         .kills = 10,
         .time_seconds = 60.0f},
        {.date = test_date(2026, 8, 23),
         .level_name = "another-level",
         .outcome = LevelOutcome::Victory,
         .kills = 100,
         .time_seconds = 1.0f},
    };

    auto const summary{summarize_level_progress(level_id, records)};
    EXPECT_TRUE(summary.state() == LevelProgressState::Completed) << "Completed level is reported";
    EXPECT_EQ(summary.attempt_count, 3) << "Every matching attempt is counted";
    EXPECT_EQ(summary.completion_count, 2) << "Successful attempts are counted";
    EXPECT_EQ(summary.best_kills, 12) << "Best kill count is retained";
    EXPECT_EQ(summary.best_completion_time_seconds, 60.0f)
        << "Fastest successful completion is retained";
    EXPECT_TRUE(summary.last_played_at == test_date(2026, 8, 22))
        << "Latest matching attempt is retained";
}

TEST(LevelProgress, DistinguishesAttemptedAndUnknownLevels) {

    std::vector<ScoreRecord> const records{
        {.level_name = "failed-level", .outcome = LevelOutcome::Defeat, .kills = 3}};

    auto const attempted{
        summarize_level_progress(levels::LevelId{std::string{"failed-level"}}, records)};
    EXPECT_TRUE(attempted.state() == LevelProgressState::Attempted) << "Failed level is attempted";
    EXPECT_EQ(attempted.completion_count, 0) << "Failed level has no completions";
    EXPECT_EQ(attempted.best_completion_time_seconds, -1.0f)
        << "Failed level has no completion time";

    auto const unknown{
        summarize_level_progress(levels::LevelId{std::string{"unknown-level"}}, records)};
    EXPECT_TRUE(unknown.state() == LevelProgressState::NotAttempted)
        << "Unknown level is not attempted";
    EXPECT_EQ(unknown.attempt_count, 0) << "Unknown level has no attempts";
}

TEST(LevelProgress, CompletionIndicatorIncludesTheParBoundary) {
    LevelProgressSummary progress;
    EXPECT_EQ(level_completion_indicator_state(progress, 75.f),
              CompletionIndicatorState::Incomplete);
    progress.completion_count = 1;
    progress.best_completion_time_seconds = 80.f;
    EXPECT_EQ(level_completion_indicator_state(progress, 75.f),
              CompletionIndicatorState::Completed);
    progress.best_completion_time_seconds = 75.f;
    EXPECT_EQ(level_completion_indicator_state(progress, 75.f),
              CompletionIndicatorState::ParAchieved);
    EXPECT_EQ(level_completion_indicator_state(progress, std::nullopt),
              CompletionIndicatorState::Completed);
    progress.best_completion_time_seconds = -1.f;
    EXPECT_EQ(level_completion_indicator_state(progress, 75.f),
              CompletionIndicatorState::Completed);
}

auto unlock_definition(std::vector<levels::LevelId> ids) -> levels::LevelDefinition {
    return levels::LevelDefinition::make(
               {.metadata = {.id = levels::LevelId{"test"}, .title = "Test"},
                .unlock_level_ids = std::move(ids),
                .camera = levels::LevelCameraDefinition{.target_entity_ids = {levels::EntityId{
                                                            "capital"}},
                                                        .offset_direction = {1., 0., 0.},
                                                        .distance = 1000.},
                .entities = {{.id = levels::EntityId{"capital"},
                              .archetype = levels::EntityArchetype::CapitalShip,
                              .team = Team::Blue}}})
        .value();
}
TEST(LevelUnlock, NoCriteriaRequiresNoCompletionQueries) {
    EXPECT_TRUE(evaluate_unlock(unlock_definition({}), [](auto const&) {
                    ADD_FAILURE();
                    return false;
                }).unlocked());
}
TEST(LevelUnlock, AllPrerequisitesMustPassAndOverridePreservesTheirStatus) {
    auto const definition{unlock_definition({levels::LevelId{"first"}, levels::LevelId{"second"}})};
    auto completed = [](levels::LevelId const& id) { return id.value == "first"; };
    auto const locked{evaluate_unlock(definition, completed)};
    ASSERT_EQ(locked.criteria.size(), 2u);
    EXPECT_FALSE(locked.unlocked());
    EXPECT_TRUE(locked.criteria[0].satisfied);
    EXPECT_FALSE(locked.criteria[1].satisfied);
    EXPECT_EQ(locked.criteria[1].level_id.value, "second");
    EXPECT_TRUE(locked.unlocked(true));
    EXPECT_FALSE(locked.criteria[1].satisfied);
    EXPECT_TRUE(evaluate_unlock(definition, [](auto const&) { return true; }).unlocked());
    EXPECT_FALSE(evaluate_unlock(definition, completed).unlocked());
}

}
