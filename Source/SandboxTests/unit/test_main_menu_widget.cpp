#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/ui/main_menu/LevelSelectWidget.h>
#include <SpaceGame/ui/main_menu/MainMenuWidget.h>
#include <SpaceGame/ui/main_menu/OptionsWidget.h>
#include <SpaceGame/ui/save_game/SaveGameViewerWidget.h>
#include <SpaceGameS7/ScriptLevelSelectWidget.h>

#include <Components/Button.h>
#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
#include <CQTest.h>

TEST_CLASS(MainMenuWidget, "Sandbox.UnitTests")
{
    TEST_METHOD(Navigation)
    {
        auto const world_result{ml::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }

        auto const widget_class{LoadClass<ml::ioj::UMainMenuWidget>(
            nullptr, TEXT("/SpaceGame/UI/MainMenu/WBP_MainMenu.WBP_MainMenu_C"))};
        if (!TestRunner->TestTrue(TEXT("Main menu class loads"), IsValid(widget_class))) {
            return;
        }

        auto* const widget{CreateWidget<ml::ioj::UMainMenuWidget>(
            world_result.value(), widget_class, TEXT("main_menu_test"))};
        if (!TestRunner->TestTrue(TEXT("Main menu is created"), IsValid(widget))) {
            return;
        }

        auto const slate_widget{widget->TakeWidget()};
        (void)slate_widget;

        auto* const play_button{Cast<UButton>(widget->GetWidgetFromName(TEXT("play_button")))};
        auto* const options_button{
            Cast<UButton>(widget->GetWidgetFromName(TEXT("options_button")))};
        auto* const save_games_button{
            Cast<UButton>(widget->GetWidgetFromName(TEXT("save_games_button")))};
        auto* const level_select_widget{Cast<ml::s7::UScriptLevelSelectWidget>(
            widget->GetWidgetFromName(TEXT("level_select_widget")))};
        auto* const options_widget{
            Cast<ml::ioj::UOptionsWidget>(widget->GetWidgetFromName(TEXT("options_widget")))};
        auto* const save_game_viewer{Cast<ml::ioj::USaveGameViewerWidget>(
            widget->GetWidgetFromName(TEXT("save_game_viewer")))};
        auto* const save_games_back_button{
            Cast<UButton>(widget->GetWidgetFromName(TEXT("save_games_back_button")))};

        auto const main_bindings_valid{IsValid(play_button) && IsValid(save_games_button) &&
                                       IsValid(options_button) && IsValid(level_select_widget) &&
                                       IsValid(save_game_viewer) &&
                                       IsValid(save_games_back_button) && IsValid(options_widget)};
        if (!TestRunner->TestTrue(TEXT("All required main menu bindings are valid"),
                                  main_bindings_valid)) {
            return;
        }

        auto* const level_back_button{
            Cast<UButton>(level_select_widget->GetWidgetFromName(TEXT("back_button")))};
        auto* const level_list{
            Cast<UVerticalBox>(level_select_widget->GetWidgetFromName(TEXT("level_list")))};
        auto* const title_text{
            Cast<UTextBlock>(level_select_widget->GetWidgetFromName(TEXT("title_text")))};
        auto* const description_text{
            Cast<UTextBlock>(level_select_widget->GetWidgetFromName(TEXT("description_text")))};
        auto* const status_text{
            Cast<UTextBlock>(level_select_widget->GetWidgetFromName(TEXT("status_text")))};
        auto* const launch_button{
            Cast<UButton>(level_select_widget->GetWidgetFromName(TEXT("launch_button")))};
        auto* const start_paused_button{
            Cast<UButton>(level_select_widget->GetWidgetFromName(TEXT("start_paused_button")))};
        auto* const video_button{
            Cast<UButton>(options_widget->GetWidgetFromName(TEXT("video_button")))};
        auto* const gameplay_button{
            Cast<UButton>(options_widget->GetWidgetFromName(TEXT("gameplay_button")))};
        auto* const audio_button{
            Cast<UButton>(options_widget->GetWidgetFromName(TEXT("audio_button")))};
        auto* const controls_button{
            Cast<UButton>(options_widget->GetWidgetFromName(TEXT("controls_button")))};
        auto* const accessibility_button{
            Cast<UButton>(options_widget->GetWidgetFromName(TEXT("accessibility_button")))};
        auto* const options_back_button{
            Cast<UButton>(options_widget->GetWidgetFromName(TEXT("back_button")))};

        auto const child_bindings_valid{
            IsValid(level_back_button) && IsValid(level_list) && IsValid(title_text) &&
            IsValid(description_text) && IsValid(status_text) && IsValid(launch_button) &&
            IsValid(start_paused_button) && IsValid(video_button) && IsValid(gameplay_button) &&
            IsValid(audio_button) && IsValid(controls_button) && IsValid(accessibility_button) &&
            IsValid(options_back_button)};
        if (!TestRunner->TestTrue(TEXT("All required child menu bindings are valid"),
                                  child_bindings_valid)) {
            return;
        }

        TestRunner->TestTrue(TEXT("Main page is active initially"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Main);

        play_button->OnClicked.Broadcast();
        TestRunner->TestTrue(TEXT("Play opens level select"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::LevelSelect);
        TestRunner->TestTrue(TEXT("Level select discovers the example script"),
                             level_list->GetChildrenCount() > 0);
        TestRunner->TestFalse(TEXT("Launch is disabled until a level is selected"),
                              launch_button->GetIsEnabled());
        TestRunner->TestFalse(TEXT("Start Paused is disabled until a level is selected"),
                              start_paused_button->GetIsEnabled());

        auto* const level_button{Cast<ml::s7::ULevelScriptButton>(level_list->GetChildAt(0))};
        if (!TestRunner->TestTrue(TEXT("Level row is selectable"), IsValid(level_button))) {
            return;
        }
        level_button->OnClicked.Broadcast();
        TestRunner->TestEqual(TEXT("Selected level title is shown"),
                              title_text->GetText().ToString(),
                              FString{TEXT("Border Skirmish")});
        TestRunner->TestTrue(
            TEXT("Selected level description is shown"),
            description_text->GetText().ToString().Contains(TEXT("two-team encounter")));
        TestRunner->TestTrue(TEXT("A valid selected level can be launched"),
                             launch_button->GetIsEnabled());
        TestRunner->TestTrue(TEXT("A valid selected level can start paused"),
                             start_paused_button->GetIsEnabled());
        TestRunner->TestEqual(TEXT("Selected level reports that it is ready"),
                              status_text->GetText().ToString(),
                              FString{TEXT("Ready to launch.")});

        level_back_button->OnClicked.Broadcast();
        TestRunner->TestTrue(TEXT("Level select Back returns to main"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Main);

        save_games_button->OnClicked.Broadcast();
        TestRunner->TestTrue(TEXT("Save Games opens save viewer"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::SaveGames);

        save_games_back_button->OnClicked.Broadcast();
        TestRunner->TestTrue(TEXT("Save Games Back returns to main"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Main);

        options_button->OnClicked.Broadcast();
        TestRunner->TestTrue(TEXT("Options opens options page"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Options);
        TestRunner->TestTrue(TEXT("Video is the initial options tab"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::Video);

        gameplay_button->OnClicked.Broadcast();
        TestRunner->TestTrue(TEXT("Gameplay tab is selectable"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::Gameplay);

        audio_button->OnClicked.Broadcast();
        TestRunner->TestTrue(TEXT("Audio tab is selectable"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::Audio);

        controls_button->OnClicked.Broadcast();
        TestRunner->TestTrue(TEXT("Controls tab is selectable"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::Controls);

        accessibility_button->OnClicked.Broadcast();
        TestRunner->TestTrue(TEXT("Accessibility tab is selectable"),
                             options_widget->get_active_tab() ==
                                 ml::ioj::EOptionsTab::Accessibility);
        TestRunner->TestTrue(TEXT("Selected tab has a distinct appearance"),
                             accessibility_button->GetBackgroundColor() !=
                                 video_button->GetBackgroundColor());

        video_button->OnClicked.Broadcast();
        TestRunner->TestTrue(TEXT("Video tab is selectable"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::Video);

        options_back_button->OnClicked.Broadcast();
        TestRunner->TestTrue(TEXT("Options Back returns to main"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Main);
    }
};
