#include <SpaceGame/persistence/SaveGameBrowser.h>

#include <CQTest.h>

namespace {
auto make_summary(FString save_id, FDateTime last_played_at, int32 const score)
    -> ml::ioj::FSaveGameSummary {
    return {
        .save_id = MoveTemp(save_id),
        .display_name = TEXT("Display name"),
        .created_at = FDateTime{2026, 1, 1},
        .last_played_at = last_played_at,
        .total_simulation_duration_seconds = 42.f,
        .aggregate_score = score,
        .outcomes = {{.outcome_id = TEXT("scenario"),
                      .display_name = TEXT("Scenario"),
                      .result = TEXT("Victory")}},
    };
}
}

TEST_CLASS(SaveGameBrowser, "Sandbox.UnitTests")
{
    TEST_METHOD(RefreshReplacesCachedSummaries)
    {
        int32 refresh_count{};
        ml::ioj::FSaveGameBrowser browser{[&refresh_count] {
            ++refresh_count;
            if (refresh_count == 1) {
                return TArray{make_summary(TEXT("first"), FDateTime{2026, 1, 1}, 10)};
            }

            return TArray{make_summary(TEXT("second"), FDateTime{2026, 1, 2}, 20)};
        }};

        browser.refresh();
        TestRunner->TestEqual(
            TEXT("First refresh has one summary"), browser.get_summaries().Num(), 1);
        TestRunner->TestEqual(TEXT("First refresh uses first result"),
                              browser.get_summaries()[0].save_id,
                              FString{TEXT("first")});

        browser.refresh();
        TestRunner->TestEqual(
            TEXT("Second refresh still has one summary"), browser.get_summaries().Num(), 1);
        TestRunner->TestEqual(TEXT("Second refresh replaces first result"),
                              browser.get_summaries()[0].save_id,
                              FString{TEXT("second")});
    }

    TEST_METHOD(EmptySourceProducesEmptyBrowser)
    {
        ml::ioj::FSaveGameBrowser browser{[] { return TArray<ml::ioj::FSaveGameSummary>{}; }};

        browser.refresh();

        TestRunner->TestTrue(TEXT("Browser is empty"), browser.get_summaries().IsEmpty());
    }

    TEST_METHOD(MultipleSummariesRemainIntact)
    {
        auto const newest{ml::ioj::FSaveGameSummary{
            .save_id = TEXT("newest"),
            .display_name = TEXT("Newest profile"),
            .created_at = FDateTime{2026, 5, 1, 10, 0},
            .last_played_at = FDateTime{2026, 5, 3, 12, 30},
            .total_simulation_duration_seconds = 123.5f,
            .aggregate_score = 9001,
            .outcomes = {{.outcome_id = TEXT("vega_defence"),
                          .display_name = TEXT("Vega Defence"),
                          .score = 9001,
                          .result = TEXT("Victory"),
                          .statistics = {{TEXT("Ships destroyed"), TEXT("12")}}}},
        }};
        auto const oldest{make_summary(TEXT("oldest"), FDateTime{2026, 5, 1}, 100)};
        ml::ioj::FSaveGameBrowser browser{[newest, oldest] { return TArray{oldest, newest}; }};

        browser.refresh();

        auto const summaries{browser.get_summaries()};
        TestRunner->TestEqual(TEXT("Both summaries remain"), summaries.Num(), 2);
        TestRunner->TestEqual(TEXT("Display name remains intact"),
                              summaries[0].display_name,
                              FString{TEXT("Newest profile")});
        TestRunner->TestTrue(TEXT("Last-played time remains intact"),
                             summaries[0].last_played_at == FDateTime{2026, 5, 3, 12, 30});
        TestRunner->TestTrue(TEXT("Duration remains intact"),
                             summaries[0].total_simulation_duration_seconds == 123.5f);
        TestRunner->TestEqual(
            TEXT("Aggregate score remains intact"), summaries[0].aggregate_score, 9001);
        TestRunner->TestEqual(TEXT("Outcome remains intact"), summaries[0].outcomes.Num(), 1);
        TestRunner->TestEqual(TEXT("Outcome statistics remain intact"),
                              summaries[0].outcomes[0].statistics[0].value,
                              FString{TEXT("12")});
    }

    TEST_METHOD(OrderIsNewestFirstAndDeterministic)
    {
        auto const shared_timestamp{FDateTime{2026, 6, 2}};
        ml::ioj::FSaveGameBrowser browser{[shared_timestamp] {
            return TArray{
                make_summary(TEXT("zulu"), shared_timestamp, 1),
                make_summary(TEXT("older"), FDateTime{2026, 6, 1}, 2),
                make_summary(TEXT("alpha"), shared_timestamp, 3),
            };
        }};

        browser.refresh();

        auto const summaries{browser.get_summaries()};
        TestRunner->TestEqual(TEXT("Alphabetical tie-breaker comes first"),
                              summaries[0].save_id,
                              FString{TEXT("alpha")});
        TestRunner->TestEqual(
            TEXT("Second tied save follows"), summaries[1].save_id, FString{TEXT("zulu")});
        TestRunner->TestEqual(
            TEXT("Older save comes last"), summaries[2].save_id, FString{TEXT("older")});
    }
};
