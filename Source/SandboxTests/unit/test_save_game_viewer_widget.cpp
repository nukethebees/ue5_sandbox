#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/persistence/SaveGameBrowser.h>
#include <SpaceGame/ui/save_game/SaveGameViewerWidget.h>

#include <CQTest.h>

namespace save_game_viewer_widget_test {
auto make_outcome(FString id, FString name) -> ml::ioj::FLevelOutcomeSummary {
    return {.outcome_id = MoveTemp(id),
            .display_name = MoveTemp(name),
            .completed_at = FDateTime{2026, 8, 27},
            .simulation_duration_seconds = 300.f,
            .kills = 5,
            .result = TEXT("Victory"),
            .statistics = {{TEXT("Accuracy"), TEXT("75%")}}};
}
} // namespace save_game_viewer_widget_test

TEST_CLASS(SaveGameViewerWidget, "Sandbox.UnitTests")
{
    TEST_METHOD(DisplaysProfilesOutcomesAndReports)
    {
        auto const world_result{ml::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }

        auto const widget_class{LoadClass<ml::ioj::USaveGameViewerWidget>(
            nullptr, TEXT("/SpaceGame/UI/SaveGame/WBP_SaveGameViewer.WBP_SaveGameViewer_C"))};
        if (!TestRunner->TestTrue(TEXT("Save game viewer class loads"), IsValid(widget_class))) {
            return;
        }

        auto* const widget{CreateWidget<ml::ioj::USaveGameViewerWidget>(
            world_result.value(), widget_class, TEXT("save_game_viewer_test"))};
        if (!TestRunner->TestTrue(TEXT("Save game viewer is created"), IsValid(widget))) {
            return;
        }

        ml::ioj::FSaveGameBrowser browser{
            [] {
                return TArray<ml::ioj::FSaveProfileSummary>{
                    {.profile_id = TEXT("battle_at_vega"),
                     .display_name = TEXT("Battle at Vega"),
                     .created_at = FDateTime{2026, 8, 20},
                     .last_played_at = FDateTime{2026, 8, 27},
                     .total_simulation_duration_seconds = 600.f,
                     .total_kills = 10,
                     .outcome_count = 2,
                     .active = true},
                    {.profile_id = TEXT("test_run_12"),
                     .display_name = TEXT("Test Run 12"),
                     .created_at = FDateTime{2026, 8, 26},
                     .last_played_at = FDateTime{2026, 8, 26},
                     .total_kills = 5,
                     .outcome_count = 1},
                    {.profile_id = TEXT("empty_profile"),
                     .display_name = TEXT("Empty Profile"),
                     .created_at = FDateTime{2026, 8, 25},
                     .last_played_at = FDateTime{2026, 8, 25}},
                };
            },
            [](FString const& profile_id) -> TOptional<ml::ioj::FSaveProfileReport> {
                if (profile_id == TEXT("battle_at_vega")) {
                    return ml::ioj::FSaveProfileReport{
                        .profile_id = profile_id,
                        .outcomes = {save_game_viewer_widget_test::make_outcome(
                                         TEXT("patrol"), TEXT("Vega Patrol")),
                                     save_game_viewer_widget_test::make_outcome(
                                         TEXT("defence"), TEXT("Vega Defence"))}};
                }
                if (profile_id == TEXT("test_run_12")) {
                    return ml::ioj::FSaveProfileReport{
                        .profile_id = profile_id,
                        .outcomes = {save_game_viewer_widget_test::make_outcome(
                            TEXT("interception"), TEXT("Fleet Interception"))}};
                }
                if (profile_id == TEXT("empty_profile")) {
                    return ml::ioj::FSaveProfileReport{.profile_id = profile_id};
                }
                return {};
            }};
        browser.refresh();
        widget->set_browser(browser);

        auto const slate_widget{widget->TakeWidget()};
        (void)slate_widget;

        TestRunner->TestEqual(
            TEXT("All service records are listed"), widget->get_profile_count(), 3);
        TestRunner->TestEqual(
            TEXT("Selected profile outcomes are indexed"), widget->get_outcome_count(), 2);
        TestRunner->TestEqual(TEXT("Newest profile is selected initially"),
                              widget->get_selected_profile_name().ToString(),
                              FString{TEXT("Battle at Vega")});
        TestRunner->TestEqual(TEXT("First report is selected initially"),
                              widget->get_selected_outcome_id(),
                              FString{TEXT("patrol")});
        TestRunner->TestFalse(TEXT("Active profile cannot be activated again"),
                              widget->can_activate_selected_profile());
        TestRunner->TestTrue(TEXT("Viewer is the CommonUI focus bridge"),
                             widget->get_focus_target() == widget);

        widget->begin_create_profile();
        TestRunner->TestTrue(TEXT("Create profile form opens"), widget->is_create_profile_open());
        widget->request_back();
        TestRunner->TestFalse(TEXT("Create profile form cancels"),
                              widget->is_create_profile_open());

        widget->select_outcome(TEXT("defence"));
        TestRunner->TestEqual(TEXT("Outcome selection updates the report"),
                              widget->get_selected_outcome_id(),
                              FString{TEXT("defence")});

        widget->select_profile(TEXT("test_run_12"));
        TestRunner->TestEqual(TEXT("Selecting a profile updates its report"),
                              widget->get_selected_profile_name().ToString(),
                              FString{TEXT("Test Run 12")});
        TestRunner->TestEqual(
            TEXT("Outcome index is rebuilt for profile"), widget->get_outcome_count(), 1);
        TestRunner->TestTrue(TEXT("Inactive profile can be explicitly activated"),
                             widget->can_activate_selected_profile());

        widget->select_profile(TEXT("empty_profile"));
        TestRunner->TestEqual(
            TEXT("Empty profile has no outcome rows"), widget->get_outcome_count(), 0);
        TestRunner->TestTrue(TEXT("Empty profile has no selected report"),
                             widget->get_selected_outcome_id().IsEmpty());

        widget->refresh();
        TestRunner->TestEqual(
            TEXT("Refresh rebuilds without duplication"), widget->get_profile_count(), 3);
        TestRunner->TestEqual(TEXT("Refresh preserves the selected profile"),
                              widget->get_selected_profile_name().ToString(),
                              FString{TEXT("Empty Profile")});
    }
};
