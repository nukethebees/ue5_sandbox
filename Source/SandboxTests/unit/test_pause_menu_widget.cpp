#include <SandboxTests/support/test_setup.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ui/main_menu/OptionsWidget.h>
#include <SpaceGame/ui/PauseMenuWidget.h>
#include <SpaceGamePresentation/ui/common/MenuButtonWidget.h>

#include <CommonInputSettings.h>
#include <Components/TextBlock.h>
#include <CQTest.h>
#include <InputAction.h>

TEST_CLASS(PauseMenuWidget, "Sandbox.UnitTests")
{
    TEST_METHOD(NavigationAndResume)
    {
        auto const world_result{ioj::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }

        auto* const ui_data{ioj::test_batch_game_ui_data::get_data_asset()};
        if (!TestRunner->TestTrue(TEXT("Project UI data loads"), IsValid(ui_data))) {
            return;
        }

        auto const widget_class{ui_data->get_widget_class<ioj::UPauseMenuWidget>()};
        if (!TestRunner->TestTrue(TEXT("Pause menu class is configured"),
                                  static_cast<bool>(widget_class))) {
            return;
        }

        auto* const widget{CreateWidget<ioj::UPauseMenuWidget>(
            world_result.value(), widget_class, TEXT("pause_menu_test"))};
        if (!TestRunner->TestTrue(TEXT("Pause menu is created"), IsValid(widget))) {
            return;
        }

        auto* const pause_action{LoadObject<UInputAction>(
            nullptr, TEXT("/SpaceGame/Input/SpaceShip/IA_pause.IA_pause"))};
        if (!TestRunner->TestTrue(TEXT("Pause action loads"), IsValid(pause_action))) {
            return;
        }

        widget->prepare_for_open(*pause_action);

        FCommonInputBase::GetInputSettings()->LoadData();
        auto const slate_widget{widget->TakeWidget()};
        (void)slate_widget;
        widget->ActivateWidget();

        auto const get_button{[widget](FName const name) {
            return Cast<ioj::UMenuButtonWidget>(widget->GetWidgetFromName(name));
        }};
        auto const get_text{[widget](FName const name) {
            return Cast<UTextBlock>(widget->GetWidgetFromName(name));
        }};
        auto* const resume_button{get_button(TEXT("resume_button"))};
        auto* const audio_button{get_button(TEXT("audio_button"))};
        auto* const controls_button{get_button(TEXT("controls_button"))};
        auto* const return_button{get_button(TEXT("return_to_level_select_button"))};
        auto* const quit_button{get_button(TEXT("quit_button"))};
        auto* const page_heading{get_text(TEXT("page_heading"))};
        auto* const options_widget{
            Cast<ioj::UOptionsWidget>(widget->GetWidgetFromName(TEXT("options_widget")))};
        if (!TestRunner->TestTrue(TEXT("Required pause menu bindings are valid"),
                                  IsValid(resume_button) && IsValid(audio_button) &&
                                      IsValid(controls_button) && IsValid(return_button) &&
                                      IsValid(quit_button) && IsValid(page_heading) &&
                                      IsValid(options_widget))) {
            return;
        }

        TestRunner->TestTrue(TEXT("Resume is the initial focus target"),
                             widget->GetDesiredFocusTarget() == resume_button);
        TestRunner->TestEqual(TEXT("Options heading is displayed"),
                              page_heading->GetText().ToString(),
                              FString{TEXT("Options")});
        TestRunner->TestTrue(TEXT("Audio is the initial options tab"),
                             options_widget->get_active_tab() == ioj::EOptionsTab::Audio);
        controls_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Controls selects the Controls tab"),
                             options_widget->get_active_tab() == ioj::EOptionsTab::Controls);
        audio_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Audio selects the Audio tab"),
                             options_widget->get_active_tab() == ioj::EOptionsTab::Audio);

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
