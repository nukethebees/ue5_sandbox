#include <ioj/persistence/save_browser.h>
#include <SandboxTests/support/test_setup.h>
#include <SpaceGame/ui/save_game/SaveGameViewerWidget.h>

#include <CQTest.h>

namespace save_game_viewer_widget_test {
auto make_outcome(std::string id, std::string name) -> ioj::persistence::LevelOutcomeSummary {
    return {.outcome_id = std::move(id),
            .display_name = std::move(name),
            .completed_at = FDateTime{2026, 8, 27}.ToUnixTimestamp(),
            .simulation_duration_seconds = 300.f,
            .kills = 5,
            .result = "Victory",
            .statistics = {{"Accuracy", "75%"}}};
}
} // namespace save_game_viewer_widget_test

TEST_CLASS(SaveGameViewerWidget, "Sandbox.UnitTests")
{
    TEST_METHOD(DisplaysProfilesOutcomesAndReports)
    {
        auto const world_result{ioj::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }

        auto const widget_class{LoadClass<ioj::USaveGameViewerWidget>(
            nullptr, TEXT("/SpaceGame/UI/SaveGame/WBP_SaveGameViewer.WBP_SaveGameViewer_C"))};
        if (!TestRunner->TestTrue(TEXT("Save game viewer class loads"), IsValid(widget_class))) {
            return;
        }

        auto* const widget{CreateWidget<ioj::USaveGameViewerWidget>(
            world_result.value(), widget_class, TEXT("save_game_viewer_test"))};
        if (!TestRunner->TestTrue(TEXT("Save game viewer is created"), IsValid(widget))) {
            return;
        }

        ioj::persistence::SaveGameBrowser browser{
            [] {
                return std::vector<ioj::persistence::ProfileSummary>{
                    {.profile_id = "battle_at_vega",
                     .display_name = "Battle at Vega",
                     .created_at = FDateTime{2026, 8, 20}.ToUnixTimestamp(),
                     .last_played_at = FDateTime{2026, 8, 27}.ToUnixTimestamp(),
                     .total_simulation_duration_seconds = 600.f,
                     .total_kills = 10,
                     .outcome_count = 2,
                     .active = true},
                    {.profile_id = "test_run_12",
                     .display_name = "Test Run 12",
                     .created_at = FDateTime{2026, 8, 26}.ToUnixTimestamp(),
                     .last_played_at = FDateTime{2026, 8, 26}.ToUnixTimestamp(),
                     .total_kills = 5,
                     .outcome_count = 1},
                    {.profile_id = "empty_profile",
                     .display_name = "Empty Profile",
                     .created_at = FDateTime{2026, 8, 25}.ToUnixTimestamp(),
                     .last_played_at = FDateTime{2026, 8, 25}.ToUnixTimestamp()},
                };
            },
            [](std::string const& profile_id) -> std::optional<ioj::persistence::ProfileReport> {
                if (profile_id == "battle_at_vega") {
                    return ioj::persistence::ProfileReport{
                        .profile_id = profile_id,
                        .outcomes = {
                            save_game_viewer_widget_test::make_outcome("patrol", "Vega Patrol"),
                            save_game_viewer_widget_test::make_outcome("defence", "Vega Defence")}};
                }
                if (profile_id == "test_run_12") {
                    return ioj::persistence::ProfileReport{
                        .profile_id = profile_id,
                        .outcomes = {save_game_viewer_widget_test::make_outcome(
                            "interception", "Fleet Interception")}};
                }
                if (profile_id == "empty_profile") {
                    return ioj::persistence::ProfileReport{.profile_id = profile_id};
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
