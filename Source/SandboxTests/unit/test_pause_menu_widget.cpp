#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ui/common/MenuButtonWidget.h>
#include <SpaceGame/ui/PauseMenuWidget.h>

#include <CommonInputSettings.h>
#include <Components/Button.h>
#include <Components/TextBlock.h>
#include <CQTest.h>

TEST_CLASS(PauseMenuWidget, "Sandbox.UnitTests")
{
    TEST_METHOD(NavigationAndResume)
    {
        auto const world_result{ml::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }

        auto* const ui_data{ml::test_batch_game_ui_data::get_data_asset()};
        if (!TestRunner->TestTrue(TEXT("Project UI data loads"), IsValid(ui_data))) {
            return;
        }

        auto const widget_class{ui_data->get_widget_class<ml::ioj::UPauseMenuWidget>()};
        if (!TestRunner->TestTrue(TEXT("Pause menu class is configured"),
                                  static_cast<bool>(widget_class))) {
            return;
        }

        auto* const widget{CreateWidget<ml::ioj::UPauseMenuWidget>(
            world_result.value(), widget_class, TEXT("pause_menu_test"))};
        if (!TestRunner->TestTrue(TEXT("Pause menu is created"), IsValid(widget))) {
            return;
        }

        FCommonInputBase::GetInputSettings()->LoadData();
        auto const slate_widget{widget->TakeWidget()};
        (void)slate_widget;
        widget->ActivateWidget();

        auto* const resume_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("resume_button")))};
        auto* const overview_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("overview_button")))};
        auto* const stats_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("stats_button")))};
        auto* const options_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("options_button")))};
        auto* const return_button{Cast<ml::ioj::UMenuButtonWidget>(
            widget->GetWidgetFromName(TEXT("return_to_level_select_button")))};
        auto* const quit_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("quit_button")))};
        auto* const page_heading{Cast<UTextBlock>(widget->GetWidgetFromName(TEXT("page_heading")))};
        auto* const page_placeholder{
            Cast<UTextBlock>(widget->GetWidgetFromName(TEXT("page_placeholder")))};

        auto const bindings_valid{IsValid(resume_button) && IsValid(overview_button) &&
                                  IsValid(stats_button) && IsValid(options_button) &&
                                  IsValid(return_button) && IsValid(quit_button) &&
                                  IsValid(page_heading) && IsValid(page_placeholder)};
        if (!TestRunner->TestTrue(TEXT("All required pause menu bindings are valid"),
                                  bindings_valid)) {
            return;
        }

        TestRunner->TestTrue(TEXT("Overview is active initially"),
                             widget->get_active_tab() == ml::ioj::EPauseMenuTab::Overview);
        TestRunner->TestTrue(TEXT("Resume is the deterministic initial focus target"),
                             widget->GetDesiredFocusTarget() == resume_button);
        TestRunner->TestEqual(TEXT("Overview heading is displayed"),
                              page_heading->GetText().ToString(),
                              TEXT("Overview"));

        stats_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Stats button activates Stats"),
                             widget->get_active_tab() == ml::ioj::EPauseMenuTab::Stats);
        TestRunner->TestEqual(
            TEXT("Stats heading is displayed"), page_heading->GetText().ToString(), TEXT("Stats"));
        TestRunner->TestTrue(TEXT("Stats placeholder is displayed"),
                             page_placeholder->GetText().ToString().Contains(TEXT("Stats")));
        TestRunner->TestTrue(TEXT("Stats has a distinct selected appearance"),
                             stats_button->GetSelected() && !overview_button->GetSelected());

        options_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Options button activates Options"),
                             widget->get_active_tab() == ml::ioj::EPauseMenuTab::Options);
        TestRunner->TestEqual(TEXT("Options heading is displayed"),
                              page_heading->GetText().ToString(),
                              TEXT("Options"));

        overview_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Overview button returns to Overview"),
                             widget->get_active_tab() == ml::ioj::EPauseMenuTab::Overview);

        resume_button->OnClicked().Broadcast();
        TestRunner->TestFalse(TEXT("Resume deactivates the pause menu"), widget->IsActivated());

        int32 return_requests{0};
        widget->return_to_level_select_requested.AddLambda(
            [&return_requests] { ++return_requests; });
        return_button->OnClicked().Broadcast();
        return_button->OnClicked().Broadcast();
        TestRunner->TestEqual(
            TEXT("Return to level select is emitted only once"), return_requests, 1);
    }
};
