#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/persistence/SaveGameBrowser.h>
#include <SpaceGame/ui/save_game/SaveGameRowWidget.h>
#include <SpaceGame/ui/save_game/SaveGameViewerWidget.h>

#include <Components/Button.h>
#include <Components/ScrollBox.h>
#include <Components/TextBlock.h>
#include <CQTest.h>

TEST_CLASS(SaveGameViewerWidget, "Sandbox.UnitTests")
{
    TEST_METHOD(DisplaysSelectsAndRefreshesMockSummaries)
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

        ml::ioj::FSaveGameBrowser browser{[] {
            return TArray<ml::ioj::FSaveGameSummary>{
                {.save_id = TEXT("battle_at_vega"),
                 .display_name = TEXT("Battle at Vega"),
                 .timestamp = FDateTime{2026, 8, 27},
                 .scenario_name = TEXT("Vega Defence")},
                {.save_id = TEXT("test_run_12"),
                 .display_name = TEXT("Test Run 12"),
                 .timestamp = FDateTime{2026, 8, 26},
                 .scenario_name = TEXT("Fleet Interception")},
                {.save_id = TEXT("fleet_benchmark"),
                 .display_name = TEXT("Fleet Benchmark"),
                 .timestamp = FDateTime{2026, 8, 25},
                 .scenario_name = TEXT("Capital Fleet Benchmark")},
            };
        }};
        browser.refresh();
        widget->set_browser(browser);

        auto const slate_widget{widget->TakeWidget()};
        (void)slate_widget;

        auto* const save_list{Cast<UScrollBox>(widget->GetWidgetFromName(TEXT("save_list")))};
        auto* const empty_state{
            Cast<UTextBlock>(widget->GetWidgetFromName(TEXT("empty_state_text")))};
        auto* const detail_name{
            Cast<UTextBlock>(widget->GetWidgetFromName(TEXT("detail_name_text")))};
        auto* const refresh_button{
            Cast<UButton>(widget->GetWidgetFromName(TEXT("refresh_button")))};
        auto const bindings_valid{IsValid(save_list) && IsValid(empty_state) &&
                                  IsValid(detail_name) && IsValid(refresh_button)};
        if (!TestRunner->TestTrue(TEXT("Viewer bindings are valid"), bindings_valid)) {
            return;
        }

        TestRunner->TestEqual(TEXT("All mock saves are listed"), save_list->GetChildrenCount(), 3);
        TestRunner->TestTrue(TEXT("Non-empty viewer hides empty state"),
                             empty_state->GetVisibility() == ESlateVisibility::Collapsed);
        TestRunner->TestEqual(TEXT("Newest save is selected initially"),
                              detail_name->GetText().ToString(),
                              FString{TEXT("Battle at Vega")});

        auto* const second_row{Cast<ml::ioj::USaveGameRowWidget>(save_list->GetChildAt(1))};
        if (!TestRunner->TestTrue(TEXT("Second save row exists"), IsValid(second_row))) {
            return;
        }

        auto* const second_row_button{
            Cast<UButton>(second_row->GetWidgetFromName(TEXT("row_button")))};
        if (!TestRunner->TestTrue(TEXT("Second row button exists"), IsValid(second_row_button))) {
            return;
        }

        second_row_button->OnClicked.Broadcast();
        TestRunner->TestEqual(TEXT("Selecting a row updates details"),
                              detail_name->GetText().ToString(),
                              FString{TEXT("Test Run 12")});

        refresh_button->OnClicked.Broadcast();
        TestRunner->TestEqual(
            TEXT("Refresh rebuilds without duplication"), save_list->GetChildrenCount(), 3);
        TestRunner->TestEqual(TEXT("Refresh preserves the selected save"),
                              detail_name->GetText().ToString(),
                              FString{TEXT("Test Run 12")});
    }
};
