#include <SpaceGame/persistence/SaveGameBrowser.h>

#include <CQTest.h>

namespace {
auto make_summary(FString save_id, FDateTime timestamp, int32 const score)
    -> ml::ioj::FSaveGameSummary {
    return {
        .save_id = MoveTemp(save_id),
        .display_name = TEXT("Display name"),
        .timestamp = timestamp,
        .scenario_name = TEXT("Scenario"),
        .simulation_duration_seconds = 42.f,
        .score = score,
        .result = TEXT("Victory"),
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
            .display_name = TEXT("Newest save"),
            .timestamp = FDateTime{2026, 5, 3, 12, 30},
            .scenario_name = TEXT("Vega Defence"),
            .simulation_duration_seconds = 123.5f,
            .score = 9001,
            .result = TEXT("Victory"),
        }};
        auto const oldest{make_summary(TEXT("oldest"), FDateTime{2026, 5, 1}, 100)};
        ml::ioj::FSaveGameBrowser browser{[newest, oldest] { return TArray{oldest, newest}; }};

        browser.refresh();

        auto const summaries{browser.get_summaries()};
        TestRunner->TestEqual(TEXT("Both summaries remain"), summaries.Num(), 2);
        TestRunner->TestEqual(TEXT("Display name remains intact"),
                              summaries[0].display_name,
                              FString{TEXT("Newest save")});
        TestRunner->TestTrue(TEXT("Timestamp remains intact"),
                             summaries[0].timestamp == FDateTime{2026, 5, 3, 12, 30});
        TestRunner->TestTrue(TEXT("Scenario remains intact"),
                             summaries[0].scenario_name == FName{TEXT("Vega Defence")});
        TestRunner->TestTrue(TEXT("Duration remains intact"),
                             summaries[0].simulation_duration_seconds == 123.5f);
        TestRunner->TestEqual(TEXT("Score remains intact"), summaries[0].score, 9001);
        TestRunner->TestEqual(
            TEXT("Result remains intact"), summaries[0].result, FString{TEXT("Victory")});
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
