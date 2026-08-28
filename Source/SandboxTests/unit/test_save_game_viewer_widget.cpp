#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/persistence/SaveGameBrowser.h>
#include <SpaceGame/ui/save_game/LevelOutcomeRowWidget.h>
#include <SpaceGame/ui/save_game/SaveGameRowWidget.h>
#include <SpaceGame/ui/save_game/SaveGameViewerWidget.h>

#include <Components/Button.h>
#include <Components/ScrollBox.h>
#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
#include <CQTest.h>

namespace {
auto make_outcome(FString id, FString name) -> ml::ioj::FLevelOutcomeSummary {
    return {.outcome_id = MoveTemp(id),
            .display_name = MoveTemp(name),
            .completed_at = FDateTime{2026, 8, 27},
            .simulation_duration_seconds = 300.f,
            .kills = 5,
            .result = TEXT("Victory"),
            .statistics = {{TEXT("Accuracy"), TEXT("75%")}}};
}
}

TEST_CLASS(SaveGameViewerWidget, "Sandbox.UnitTests")
{
    TEST_METHOD(DisplaysProfilesOutcomesAndScrollableResults)
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
                     .outcome_count = 2},
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
                        .outcomes = {make_outcome(TEXT("patrol"), TEXT("Vega Patrol")),
                                     make_outcome(TEXT("defence"), TEXT("Vega Defence"))}};
                }
                if (profile_id == TEXT("test_run_12")) {
                    return ml::ioj::FSaveProfileReport{
                        .profile_id = profile_id,
                        .outcomes = {
                            make_outcome(TEXT("interception"), TEXT("Fleet Interception"))}};
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

        auto* const profile_list{Cast<UScrollBox>(widget->GetWidgetFromName(TEXT("profile_list")))};
        auto* const outcome_list{Cast<UScrollBox>(widget->GetWidgetFromName(TEXT("outcome_list")))};
        auto* const profile_name{
            Cast<UTextBlock>(widget->GetWidgetFromName(TEXT("profile_name_text")))};
        auto* const outcome_empty_state{
            Cast<UTextBlock>(widget->GetWidgetFromName(TEXT("outcome_empty_state_text")))};
        auto* const result_sections{
            Cast<UVerticalBox>(widget->GetWidgetFromName(TEXT("result_sections_box")))};
        auto* const refresh_button{
            Cast<UButton>(widget->GetWidgetFromName(TEXT("refresh_button")))};
        auto const bindings_valid{IsValid(profile_list) && IsValid(outcome_list) &&
                                  IsValid(profile_name) && IsValid(outcome_empty_state) &&
                                  IsValid(result_sections) && IsValid(refresh_button)};
        if (!TestRunner->TestTrue(TEXT("Viewer bindings are valid"), bindings_valid)) {
            return;
        }

        TestRunner->TestEqual(TEXT("All profiles are listed"), profile_list->GetChildrenCount(), 3);
        TestRunner->TestEqual(
            TEXT("Selected profile outcomes are indexed"), outcome_list->GetChildrenCount(), 2);
        TestRunner->TestEqual(TEXT("Every outcome has a full result section"),
                              result_sections->GetChildrenCount(),
                              2);
        TestRunner->TestEqual(TEXT("Newest profile is selected initially"),
                              profile_name->GetText().ToString(),
                              FString{TEXT("Battle at Vega")});

        auto* const second_outcome{
            Cast<ml::ioj::ULevelOutcomeRowWidget>(outcome_list->GetChildAt(1))};
        auto* const second_outcome_button{
            second_outcome ? Cast<UButton>(second_outcome->GetWidgetFromName(TEXT("row_button")))
                           : nullptr};
        if (!TestRunner->TestTrue(TEXT("Second outcome row is navigable"),
                                  IsValid(second_outcome_button))) {
            return;
        }
        second_outcome_button->OnClicked.Broadcast();
        auto* const first_outcome{
            Cast<ml::ioj::ULevelOutcomeRowWidget>(outcome_list->GetChildAt(0))};
        auto* const first_outcome_button{
            first_outcome ? Cast<UButton>(first_outcome->GetWidgetFromName(TEXT("row_button")))
                          : nullptr};
        if (!TestRunner->TestTrue(TEXT("First outcome row is navigable"),
                                  IsValid(first_outcome_button))) {
            return;
        }
        TestRunner->TestTrue(TEXT("Jump target is shown as the selected outcome"),
                             first_outcome_button->GetBackgroundColor() !=
                                 second_outcome_button->GetBackgroundColor());

        auto* const second_profile{Cast<ml::ioj::USaveGameRowWidget>(profile_list->GetChildAt(1))};
        auto* const second_profile_button{
            second_profile ? Cast<UButton>(second_profile->GetWidgetFromName(TEXT("row_button")))
                           : nullptr};
        if (!TestRunner->TestTrue(TEXT("Second profile row is navigable"),
                                  IsValid(second_profile_button))) {
            return;
        }
        second_profile_button->OnClicked.Broadcast();
        TestRunner->TestEqual(TEXT("Selecting a profile updates its report"),
                              profile_name->GetText().ToString(),
                              FString{TEXT("Test Run 12")});
        TestRunner->TestEqual(
            TEXT("Outcome index is rebuilt for profile"), outcome_list->GetChildrenCount(), 1);
        TestRunner->TestEqual(
            TEXT("Full results are rebuilt for profile"), result_sections->GetChildrenCount(), 1);

        auto* const empty_profile{Cast<ml::ioj::USaveGameRowWidget>(profile_list->GetChildAt(2))};
        auto* const empty_profile_button{
            empty_profile ? Cast<UButton>(empty_profile->GetWidgetFromName(TEXT("row_button")))
                          : nullptr};
        if (!TestRunner->TestTrue(TEXT("Empty profile row is navigable"),
                                  IsValid(empty_profile_button))) {
            return;
        }
        empty_profile_button->OnClicked.Broadcast();
        TestRunner->TestEqual(
            TEXT("Empty profile has no outcome rows"), outcome_list->GetChildrenCount(), 0);
        TestRunner->TestEqual(
            TEXT("Empty profile has no result sections"), result_sections->GetChildrenCount(), 0);
        TestRunner->TestTrue(TEXT("Empty profile explains the missing outcomes"),
                             outcome_empty_state->GetVisibility() == ESlateVisibility::Visible);

        refresh_button->OnClicked.Broadcast();
        TestRunner->TestEqual(
            TEXT("Refresh rebuilds without duplication"), profile_list->GetChildrenCount(), 3);
        TestRunner->TestEqual(TEXT("Refresh preserves the selected profile"),
                              profile_name->GetText().ToString(),
                              FString{TEXT("Empty Profile")});
    }
};
