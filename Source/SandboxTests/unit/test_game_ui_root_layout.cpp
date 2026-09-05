#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ui/common/GameUiRootLayout.h>
#include <SpaceGame/ui/common/MenuButtonWidget.h>
#include <SpaceGame/ui/LevelCompletionWidget.h>
#include <SpaceGame/ui/main_menu/LevelSelectWidget.h>
#include <SpaceGame/ui/main_menu/MainMenuWidget.h>
#include <SpaceGame/ui/PauseMenuWidget.h>

#include <CommonInputSettings.h>
#include <CommonUISettings.h>
#include <CQTest.h>
#include <Input/UIActionBindingHandle.h>
#include <InputAction.h>

TEST_CLASS(GameUiRootLayout, "Sandbox.UnitTests")
{
    TEST_METHOD(ScreenAndModalStacks)
    {
        auto const world_result{ml::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }

        auto* const ui_data{ml::test_batch_game_ui_data::get_data_asset()};
        auto const root_class{
            IsValid(ui_data) ? ui_data->get_widget_class<ml::ioj::UGameUiRootLayout>() : nullptr};
        auto* const root{
            root_class ? CreateWidget<ml::ioj::UGameUiRootLayout>(world_result.value(), root_class)
                       : nullptr};
        if (!TestRunner->TestTrue(TEXT("Configured root layout is created"), IsValid(root)) ||
            !TestRunner->TestTrue(TEXT("Root layout initialises"), root->initialise(*ui_data))) {
            return;
        }

        FCommonInputBase::GetInputSettings()->LoadData();
        TestRunner->TestTrue(TEXT("Common buttons use Slate's normal focused Accept path"),
                             GetDefault<UCommonUISettings>()->GetCommonButtonAcceptKeyHandling() ==
                                 ECommonButtonAcceptKeyHandling::TriggerClick);

        auto const root_slate{root->TakeWidget()};
        (void)root_slate;
        root->ActivateWidget();

        auto const root_input_config{root->GetDesiredInputConfig()};
        TestRunner->TestTrue(TEXT("The root restores gameplay input when no menu is active"),
                             root_input_config.IsSet() &&
                                 root_input_config->GetInputMode() == ECommonInputMode::Game);

        TestRunner->TestTrue(TEXT("Main menu is pushed"), root->show_main_menu(false));
        auto* const main_menu{Cast<ml::ioj::UMainMenuWidget>(root->get_active_screen())};
        auto* const play_button{IsValid(main_menu)
                                    ? Cast<ml::ioj::UMenuButtonWidget>(
                                          main_menu->GetWidgetFromName(TEXT("play_button")))
                                    : nullptr};
        if (!TestRunner->TestTrue(TEXT("Main menu and Play are active"),
                                  IsValid(main_menu) && IsValid(play_button))) {
            return;
        }
        auto const menu_input_config{main_menu->GetDesiredInputConfig()};
        TestRunner->TestTrue(
            TEXT("Active menus own menu input without mouse capture"),
            menu_input_config.IsSet() &&
                menu_input_config->GetInputMode() == ECommonInputMode::Menu &&
                menu_input_config->GetMouseCaptureMode() == EMouseCaptureMode::NoCapture &&
                menu_input_config->bIgnoreMoveInput && menu_input_config->bIgnoreLookInput);
        TestRunner->TestEqual(
            TEXT("The screen stack contains one menu"), root->get_screen_count(), 1);
        auto const root_menu_input_config{root->GetDesiredInputConfig()};
        TestRunner->TestTrue(TEXT("The root keeps menu input active while a normal screen exists"),
                             root_menu_input_config.IsSet() &&
                                 root_menu_input_config->GetInputMode() == ECommonInputMode::Menu &&
                                 root_menu_input_config->GetMouseCaptureMode() ==
                                     EMouseCaptureMode::NoCapture);

        play_button->OnClicked().Broadcast();
        auto* const level_select{Cast<ml::ioj::ULevelSelectWidget>(root->get_active_screen())};
        TestRunner->TestTrue(TEXT("Play pushes the level selector"), IsValid(level_select));
        TestRunner->TestEqual(
            TEXT("The screen stack contains main and level select"), root->get_screen_count(), 2);
        auto const transitioned_input_config{root->GetDesiredInputConfig()};
        TestRunner->TestTrue(
            TEXT("Pushing level select does not expose gameplay mouse capture"),
            transitioned_input_config.IsSet() &&
                transitioned_input_config->GetInputMode() == ECommonInputMode::Menu &&
                transitioned_input_config->GetMouseCaptureMode() == EMouseCaptureMode::NoCapture);
        if (IsValid(level_select)) {
            level_select->DeactivateWidget();
        }
        TestRunner->TestTrue(TEXT("Back restores the existing main menu"),
                             root->get_active_screen() == main_menu);
        TestRunner->TestEqual(
            TEXT("The level selector is removed from the stack"), root->get_screen_count(), 1);
        TestRunner->TestTrue(TEXT("Returning restores Play as the focus target"),
                             main_menu->GetDesiredFocusTarget() == play_button);

        auto* const pause_action{LoadObject<UInputAction>(
            nullptr, TEXT("/SpaceGame/Input/SpaceShip/IA_pause.IA_pause"))};
        auto* pause_menu{IsValid(pause_action) ? root->show_pause_menu(*pause_action, {})
                                               : nullptr};
        if (!TestRunner->TestTrue(TEXT("Pause menu is pushed"), IsValid(pause_menu))) {
            return;
        }
        auto* const resume_button{
            Cast<ml::ioj::UMenuButtonWidget>(pause_menu->GetWidgetFromName(TEXT("resume_button")))};
        TestRunner->TestTrue(TEXT("Resume is the pause focus target"),
                             IsValid(resume_button) &&
                                 pause_menu->GetDesiredFocusTarget() == resume_button);
        TestRunner->TestEqual(
            TEXT("The modal stack contains one pause menu"), root->get_modal_count(), 1);

        pause_menu->DeactivateWidget();
        TestRunner->TestEqual(
            TEXT("Closing pause empties the modal stack"), root->get_modal_count(), 0);
        pause_menu = root->show_pause_menu(*pause_action, {});
        TestRunner->TestTrue(TEXT("Pause can be reopened"), IsValid(pause_menu));
        TestRunner->TestEqual(
            TEXT("Reopening pause does not accumulate widgets"), root->get_modal_count(), 1);
        pause_menu->DeactivateWidget();

        auto* const completion{root->show_level_completion(TEXT("Border Skirmish"))};
        if (!TestRunner->TestTrue(TEXT("Completion is pushed"), IsValid(completion))) {
            return;
        }
        auto* const return_button{Cast<ml::ioj::UMenuButtonWidget>(
            completion->GetWidgetFromName(TEXT("return_to_level_select_button")))};
        TestRunner->TestTrue(TEXT("Return is the completion focus target"),
                             IsValid(return_button) &&
                                 completion->GetDesiredFocusTarget() == return_button);
        TestRunner->TestTrue(TEXT("Repeated completion returns the active instance"),
                             root->show_level_completion(TEXT("Ignored")) == completion);
        TestRunner->TestEqual(
            TEXT("Completion does not accumulate widgets"), root->get_modal_count(), 1);
    }
};
