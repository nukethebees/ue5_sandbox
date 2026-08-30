#include <SpaceGame/persistence/SaveGameBrowser.h>

#include <CQTest.h>

namespace save_game_browser_test {
auto make_summary(FString profile_id, FDateTime last_played_at, int32 const total_kills)
    -> ml::ioj::FSaveProfileSummary {
    return {
        .profile_id = MoveTemp(profile_id),
        .display_name = TEXT("Display name"),
        .created_at = FDateTime{2026, 1, 1},
        .last_played_at = last_played_at,
        .total_simulation_duration_seconds = 42.f,
        .total_kills = total_kills,
        .outcome_count = 1,
    };
}

auto no_report(FString const&) -> TOptional<ml::ioj::FSaveProfileReport> {
    return {};
}
}

TEST_CLASS(SaveGameBrowser, "Sandbox.UnitTests")
{
    TEST_METHOD(RefreshReplacesCachedSummaries)
    {
        int32 refresh_count{};
        ml::ioj::FSaveGameBrowser browser{
            [&refresh_count] {
                ++refresh_count;
                if (refresh_count == 1) {
                    return TArray{save_game_browser_test::make_summary(
                        TEXT("first"), FDateTime{2026, 1, 1}, 10)};
                }

                return TArray{save_game_browser_test::make_summary(
                    TEXT("second"), FDateTime{2026, 1, 2}, 20)};
            },
            save_game_browser_test::no_report};

        browser.refresh();
        TestRunner->TestEqual(
            TEXT("First refresh has one summary"), browser.get_summaries().Num(), 1);
        TestRunner->TestEqual(TEXT("First refresh uses first result"),
                              browser.get_summaries()[0].profile_id,
                              FString{TEXT("first")});

        browser.refresh();
        TestRunner->TestEqual(
            TEXT("Second refresh still has one summary"), browser.get_summaries().Num(), 1);
        TestRunner->TestEqual(TEXT("Second refresh replaces first result"),
                              browser.get_summaries()[0].profile_id,
                              FString{TEXT("second")});
    }

    TEST_METHOD(EmptySourceProducesEmptyBrowser)
    {
        ml::ioj::FSaveGameBrowser browser{[] { return TArray<ml::ioj::FSaveProfileSummary>{}; },
                                          save_game_browser_test::no_report};

        browser.refresh();

        TestRunner->TestTrue(TEXT("Browser is empty"), browser.get_summaries().IsEmpty());
    }

    TEST_METHOD(MultipleSummariesRemainIntact)
    {
        auto const newest{ml::ioj::FSaveProfileSummary{
            .profile_id = TEXT("newest"),
            .display_name = TEXT("Newest profile"),
            .created_at = FDateTime{2026, 5, 1, 10, 0},
            .last_played_at = FDateTime{2026, 5, 3, 12, 30},
            .total_simulation_duration_seconds = 123.5f,
            .total_kills = 91,
            .outcome_count = 12,
        }};
        auto const oldest{
            save_game_browser_test::make_summary(TEXT("oldest"), FDateTime{2026, 5, 1}, 10)};
        ml::ioj::FSaveGameBrowser browser{[newest, oldest] { return TArray{oldest, newest}; },
                                          save_game_browser_test::no_report};

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
        TestRunner->TestEqual(TEXT("Total kills remain intact"), summaries[0].total_kills, 91);
        TestRunner->TestEqual(TEXT("Outcome count remains intact"), summaries[0].outcome_count, 12);
    }

    TEST_METHOD(OrderIsNewestFirstAndDeterministic)
    {
        auto const shared_timestamp{FDateTime{2026, 6, 2}};
        ml::ioj::FSaveGameBrowser browser{
            [shared_timestamp] {
                return TArray{
                    save_game_browser_test::make_summary(TEXT("zulu"), shared_timestamp, 1),
                    save_game_browser_test::make_summary(TEXT("older"), FDateTime{2026, 6, 1}, 2),
                    save_game_browser_test::make_summary(TEXT("alpha"), shared_timestamp, 3),
                };
            },
            save_game_browser_test::no_report};

        browser.refresh();

        auto const summaries{browser.get_summaries()};
        TestRunner->TestEqual(TEXT("Alphabetical tie-breaker comes first"),
                              summaries[0].profile_id,
                              FString{TEXT("alpha")});
        TestRunner->TestEqual(
            TEXT("Second tied profile follows"), summaries[1].profile_id, FString{TEXT("zulu")});
        TestRunner->TestEqual(
            TEXT("Older profile comes last"), summaries[2].profile_id, FString{TEXT("older")});
    }

    TEST_METHOD(ReportsAreLoadedLazilyAndRefreshInvalidatesTheCache)
    {
        int32 report_load_count{};
        ml::ioj::FSaveGameBrowser browser{[] {
                                              return TArray{save_game_browser_test::make_summary(
                                                  TEXT("profile"), FDateTime{2026, 1, 1}, 5)};
                                          },
                                          [&report_load_count](FString const& profile_id)
                                              -> TOptional<ml::ioj::FSaveProfileReport> {
                                              ++report_load_count;
                                              if (profile_id != TEXT("profile")) {
                                                  return {};
                                              }
                                              return ml::ioj::FSaveProfileReport{
                                                  .profile_id = profile_id,
                                                  .outcomes = {{.outcome_id = TEXT("outcome")}},
                                              };
                                          }};

        browser.refresh();
        TestRunner->TestTrue(TEXT("Refresh does not load a report"), report_load_count == 0);
        TestRunner->TestTrue(TEXT("No report is initially cached"),
                             browser.get_loaded_profile_report() == nullptr);

        TestRunner->TestTrue(TEXT("Known profile report loads"),
                             browser.load_profile_report(TEXT("profile")));
        TestRunner->TestTrue(TEXT("Report source is called once"), report_load_count == 1);
        TestRunner->TestEqual(TEXT("Loaded report retains its outcome"),
                              browser.get_loaded_profile_report()->outcomes.Num(),
                              1);

        TestRunner->TestTrue(TEXT("Reloading selected profile succeeds"),
                             browser.load_profile_report(TEXT("profile")));
        TestRunner->TestTrue(TEXT("Selected report is served from cache"), report_load_count == 1);

        browser.refresh();
        TestRunner->TestTrue(TEXT("Refresh invalidates the selected report"),
                             browser.get_loaded_profile_report() == nullptr);
    }
};
