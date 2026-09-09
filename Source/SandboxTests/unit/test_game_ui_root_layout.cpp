#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/ui/common/GameUiRootLayout.h>
#include <SpaceGame/ui/LevelCompletionWidget.h>
#include <SpaceGame/ui/main_menu/LevelSelectWidget.h>
#include <SpaceGame/ui/main_menu/MainMenuWidget.h>
#include <SpaceGame/ui/PauseMenuWidget.h>
#include <SpaceGamePresentation/ui/common/MenuButtonWidget.h>

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
        if (!TestRunner->TestTrue(TEXT("Command-deck main menu is active"), IsValid(main_menu))) {
            return;
        }
        TestRunner->TestTrue(TEXT("Select Mission is the initial command-deck page"),
                             main_menu->get_active_page() == ml::ioj::EMainMenuPage::SelectMission);
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

        auto* const level_select{main_menu->get_level_select_widget()};
        TestRunner->TestTrue(TEXT("Select Mission is hosted by the command deck"),
                             IsValid(level_select));
        TestRunner->TestEqual(
            TEXT("Hosted pages do not add screens to the stack"), root->get_screen_count(), 1);
        auto const transitioned_input_config{root->GetDesiredInputConfig()};
        TestRunner->TestTrue(
            TEXT("Hosted level select does not expose gameplay mouse capture"),
            transitioned_input_config.IsSet() &&
                transitioned_input_config->GetInputMode() == ECommonInputMode::Menu &&
                transitioned_input_config->GetMouseCaptureMode() == EMouseCaptureMode::NoCapture);
        main_menu->select_page(ml::ioj::EMainMenuPage::DataArchive);
        main_menu->select_page(ml::ioj::EMainMenuPage::SelectMission);
        TestRunner->TestTrue(TEXT("Page navigation retains the existing command deck"),
                             root->get_active_screen() == main_menu &&
                                 main_menu->get_level_select_widget() == level_select);
        TestRunner->TestEqual(
            TEXT("Page navigation leaves the screen stack unchanged"), root->get_screen_count(), 1);
        TestRunner->TestTrue(TEXT("The command deck remains the CommonUI focus target"),
                             main_menu->GetDesiredFocusTarget() == main_menu);

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

        FLevelTelemetrySnapshot completion_snapshot;
        completion_snapshot.kills = 6;
        auto* const completion{root->show_level_completion(
            TEXT("Border Skirmish"), ETestMissionState::Succeeded, completion_snapshot)};
        if (!TestRunner->TestTrue(TEXT("Completion is pushed"), IsValid(completion))) {
            return;
        }
        TestRunner->TestTrue(TEXT("Completion owns its native focus target"),
                             completion->GetDesiredFocusTarget() == completion);
        TestRunner->TestTrue(TEXT("Repeated completion returns the active instance"),
                             root->show_level_completion(
                                 TEXT("Ignored"), ETestMissionState::Succeeded, {}) == completion);
        TestRunner->TestEqual(
            TEXT("Completion does not accumulate widgets"), root->get_modal_count(), 1);
    }
};
