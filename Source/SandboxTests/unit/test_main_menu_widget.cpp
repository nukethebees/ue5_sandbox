#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/ui/main_menu/LevelSelectWidget.h>
#include <SpaceGame/ui/main_menu/MainMenuWidget.h>
#include <SpaceGame/ui/main_menu/OptionsWidget.h>

#include <Components/Button.h>
#include <Components/TextBlock.h>
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
        auto* const level_select_widget{Cast<ml::ioj::ULevelSelectWidget>(
            widget->GetWidgetFromName(TEXT("level_select_widget")))};
        auto* const options_widget{
            Cast<ml::ioj::UOptionsWidget>(widget->GetWidgetFromName(TEXT("options_widget")))};

        auto const main_bindings_valid{IsValid(play_button) && IsValid(options_button) &&
                                       IsValid(level_select_widget) && IsValid(options_widget)};
        if (!TestRunner->TestTrue(TEXT("All required main menu bindings are valid"),
                                  main_bindings_valid)) {
            return;
        }

        auto* const level_back_button{
            Cast<UButton>(level_select_widget->GetWidgetFromName(TEXT("back_button")))};
        auto* const level_placeholder{
            Cast<UTextBlock>(level_select_widget->GetWidgetFromName(TEXT("placeholder_text")))};
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
            IsValid(level_back_button) && IsValid(level_placeholder) && IsValid(video_button) &&
            IsValid(gameplay_button) && IsValid(audio_button) && IsValid(controls_button) &&
            IsValid(accessibility_button) && IsValid(options_back_button)};
        if (!TestRunner->TestTrue(TEXT("All required child menu bindings are valid"),
                                  child_bindings_valid)) {
            return;
        }

        TestRunner->TestTrue(TEXT("Main page is active initially"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Main);

        play_button->OnClicked.Broadcast();
        TestRunner->TestTrue(TEXT("Play opens level select"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::LevelSelect);
        TestRunner->TestTrue(TEXT("Level select remains a placeholder"),
                             level_placeholder->GetText().ToString().Contains(TEXT("later")));

        level_back_button->OnClicked.Broadcast();
        TestRunner->TestTrue(TEXT("Level select Back returns to main"),
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
