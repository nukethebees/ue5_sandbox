#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/ui/common/MenuButtonWidget.h>
#include <SpaceGame/ui/main_menu/LevelSelectWidget.h>
#include <SpaceGame/ui/main_menu/MainMenuWidget.h>
#include <SpaceGame/ui/main_menu/OptionsWidget.h>
#include <SpaceGame/ui/save_game/SaveGameViewerWidget.h>
#include <SpaceGameS7/ScriptLevelSelectWidget.h>

#include <CommonInputSettings.h>
#include <Components/Button.h>
#include <Components/HorizontalBox.h>
#include <Components/HorizontalBoxSlot.h>
#include <Components/OverlaySlot.h>
#include <Components/ScrollBox.h>
#include <Components/TextBlock.h>
#include <Components/VerticalBox.h>
#include <CQTest.h>

TEST_CLASS(MainMenuWidget, "Sandbox.UnitTests")
{
    TEST_METHOD(Navigation)
    {
        auto const* const text_style{GetDefault<ml::ioj::UMenuTextStyle>()};
        TestRunner->TestTrue(TEXT("Common menu text style has a renderable font"),
                             IsValid(text_style) && (text_style->Font.CompositeFont.IsValid() ||
                                                     IsValid(text_style->Font.FontObject)));

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

        FCommonInputBase::GetInputSettings()->LoadData();
        auto const slate_widget{widget->TakeWidget()};
        (void)slate_widget;

        auto* const play_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("play_button")))};
        auto* const options_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("options_button")))};
        auto* const save_games_button{
            Cast<ml::ioj::UMenuButtonWidget>(widget->GetWidgetFromName(TEXT("save_games_button")))};
        auto* const options_widget{
            Cast<ml::ioj::UOptionsWidget>(widget->GetWidgetFromName(TEXT("options_widget")))};
        auto* const save_game_viewer{Cast<ml::ioj::USaveGameViewerWidget>(
            widget->GetWidgetFromName(TEXT("save_game_viewer")))};
        auto* const save_games_back_button{
            Cast<UButton>(widget->GetWidgetFromName(TEXT("save_games_back_button")))};

        auto const main_bindings_valid{IsValid(play_button) && IsValid(save_games_button) &&
                                       IsValid(options_button) && IsValid(save_game_viewer) &&
                                       IsValid(save_games_back_button) && IsValid(options_widget)};
        if (!TestRunner->TestTrue(TEXT("All required main menu bindings are valid"),
                                  main_bindings_valid)) {
            return;
        }

        auto const level_select_class{LoadClass<ml::s7::UScriptLevelSelectWidget>(
            nullptr, TEXT("/SpaceGame/UI/MainMenu/WBP_LevelSelect.WBP_LevelSelect_C"))};
        auto* const level_select_widget{IsValid(level_select_class)
                                            ? CreateWidget<ml::s7::UScriptLevelSelectWidget>(
                                                  world_result.value(), level_select_class)
                                            : nullptr};
        if (!TestRunner->TestTrue(TEXT("Level selector is created"),
                                  IsValid(level_select_widget))) {
            return;
        }
        level_select_widget->prepare_for_open(TEXT("turret-trial-0"));
        auto const level_select_slate{level_select_widget->TakeWidget()};
        (void)level_select_slate;
        level_select_widget->ActivateWidget();

        auto* const level_back_button{Cast<ml::ioj::UMenuButtonWidget>(
            level_select_widget->GetWidgetFromName(TEXT("back_button")))};
        auto* const level_list{
            Cast<UVerticalBox>(level_select_widget->GetWidgetFromName(TEXT("level_list")))};
        auto* const title_text{
            Cast<UTextBlock>(level_select_widget->GetWidgetFromName(TEXT("title_text")))};
        auto* const description_text{
            Cast<UTextBlock>(level_select_widget->GetWidgetFromName(TEXT("description_text")))};
        auto* const status_text{
            Cast<UTextBlock>(level_select_widget->GetWidgetFromName(TEXT("status_text")))};
        auto* const details_text{
            Cast<UTextBlock>(level_select_widget->GetWidgetFromName(TEXT("details_text")))};
        auto* const launch_button{Cast<ml::ioj::UMenuButtonWidget>(
            level_select_widget->GetWidgetFromName(TEXT("launch_button")))};
        auto* const start_paused_button{Cast<ml::ioj::UMenuButtonWidget>(
            level_select_widget->GetWidgetFromName(TEXT("start_paused_button")))};
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
            IsValid(description_text) && IsValid(status_text) && IsValid(details_text) &&
            IsValid(launch_button) && IsValid(start_paused_button) && IsValid(video_button) &&
            IsValid(gameplay_button) && IsValid(audio_button) && IsValid(controls_button) &&
            IsValid(accessibility_button) && IsValid(options_back_button)};
        if (!TestRunner->TestTrue(TEXT("All required child menu bindings are valid"),
                                  child_bindings_valid)) {
            return;
        }

        TestRunner->TestTrue(TEXT("Main page is active initially"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Main);
        TestRunner->TestTrue(TEXT("Play is the deterministic initial focus target"),
                             widget->GetDesiredFocusTarget() == play_button);
        auto* const play_label{
            IsValid(play_button) ? play_button->GetWidgetFromName(TEXT("label_text")) : nullptr};
        TestRunner->TestTrue(TEXT("Common button labels do not intercept mouse input"),
                             IsValid(play_label) &&
                                 play_label->GetVisibility() == ESlateVisibility::HitTestInvisible);

        bool level_select_requested{false};
        widget->level_select_requested.AddLambda(
            [&level_select_requested] { level_select_requested = true; });
        play_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Play requests level select"), level_select_requested);
        TestRunner->TestTrue(TEXT("Level select discovers the example script"),
                             level_list->GetChildrenCount() > 0);
        TestRunner->TestTrue(TEXT("Restored preferred level enables Launch"),
                             launch_button->GetIsEnabled());
        TestRunner->TestTrue(TEXT("Restored preferred level enables Start Paused"),
                             start_paused_button->GetIsEnabled());

        auto* const campaign_header{Cast<UTextBlock>(level_list->GetChildAt(0))};
        TestRunner->TestTrue(TEXT("Campaign header is not a playable level"),
                             IsValid(campaign_header));
        TestRunner->TestEqual(TEXT("First campaign is labelled"),
                              campaign_header->GetText().ToString(),
                              FString{TEXT("Scenarios")});
        TestRunner->TestEqual(TEXT("Completed level rows receive a visible marker"),
                              ml::s7::format_level_row_title(TEXT("Border Skirmish"),
                                                             ml::s7::ELevelRowState::Completed),
                              FString{TEXT("\u2713 Border Skirmish")});
        TestRunner->TestTrue(
            TEXT("Locked level rows receive a visible marker"),
            ml::s7::format_level_row_title(TEXT("Border Skirmish"), ml::s7::ELevelRowState::Locked)
                .EndsWith(TEXT("Border Skirmish")));
        TestRunner->TestEqual(
            TEXT("Invalid level rows receive an error marker"),
            ml::s7::format_level_row_title(TEXT("Broken Script"), ml::s7::ELevelRowState::Invalid),
            FString{TEXT("! Broken Script")});

        ml::ioj::UMenuButtonWidget* level_button{nullptr};
        ml::ioj::UMenuButtonWidget* turret_trial_button{nullptr};
        ml::ioj::UMenuButtonWidget* locked_turret_trial_button{nullptr};
        auto const level_row_count{level_list->GetChildrenCount()};
        for (int32 i{0}; i < level_row_count; ++i) {
            auto* const candidate{Cast<ml::ioj::UMenuButtonWidget>(level_list->GetChildAt(i))};
            if (!IsValid(candidate)) {
                continue;
            }
            if (candidate->get_text().ToString().EndsWith(TEXT("Border Skirmish"))) {
                level_button = candidate;
            } else if (candidate->get_text().ToString().EndsWith(TEXT("Turret Trial 0"))) {
                turret_trial_button = candidate;
            } else if (candidate->get_text().ToString().EndsWith(TEXT("Turret Trial 1"))) {
                locked_turret_trial_button = candidate;
            }
        }
        if (!TestRunner->TestTrue(TEXT("Level row is selectable"), IsValid(level_button))) {
            return;
        }
        TestRunner->TestTrue(TEXT("Unlocked level row remains selectable"),
                             IsValid(turret_trial_button));
        TestRunner->TestTrue(TEXT("Preferred stable level id restores row focus"),
                             level_select_widget->GetDesiredFocusTarget() == turret_trial_button);
        TestRunner->TestTrue(TEXT("Preferred stable level id restores row selection"),
                             IsValid(turret_trial_button) && turret_trial_button->GetSelected());
        TestRunner->TestEqual(TEXT("Preferred level information is displayed immediately"),
                              title_text->GetText().ToString(),
                              FString{TEXT("Turret Trial 0")});
        TestRunner->TestTrue(TEXT("Preferred valid level can be launched immediately"),
                             launch_button->GetIsEnabled());

        if (!TestRunner->TestTrue(TEXT("Locked level row remains enabled and focusable"),
                                  IsValid(locked_turret_trial_button) &&
                                      locked_turret_trial_button->GetIsEnabled())) {
            return;
        }
        locked_turret_trial_button->OnClicked().Broadcast();
        TestRunner->TestEqual(TEXT("Locked level information is displayed"),
                              title_text->GetText().ToString(),
                              FString{TEXT("Turret Trial 1")});
        TestRunner->TestTrue(TEXT("Locked level row retains focus"),
                             level_select_widget->GetDesiredFocusTarget() ==
                                 locked_turret_trial_button);
        TestRunner->TestTrue(TEXT("Locked level row can be selected"),
                             locked_turret_trial_button->GetSelected());
        TestRunner->TestFalse(TEXT("Locked level cannot be launched"),
                              launch_button->GetIsEnabled());
        TestRunner->TestFalse(TEXT("Locked level cannot start paused"),
                              start_paused_button->GetIsEnabled());
        TestRunner->TestTrue(TEXT("Locked level describes its unsatisfied requirement"),
                             description_text->GetText().ToString().Contains(
                                 TEXT("\u2610 Complete Turret Trial 0")));

        auto* const levels_scroll{Cast<UScrollBox>(level_list->GetParent())};
        auto* const levels_column{
            IsValid(levels_scroll) ? Cast<UVerticalBox>(levels_scroll->GetParent()) : nullptr};
        auto* const controls_column{Cast<UVerticalBox>(launch_button->GetParent())};
        auto* const details_column{Cast<UVerticalBox>(title_text->GetParent())};
        auto* const body{IsValid(details_column) ? Cast<UHorizontalBox>(details_column->GetParent())
                                                 : nullptr};
        auto* const page{IsValid(body) ? Cast<UVerticalBox>(body->GetParent()) : nullptr};
        auto* const controls_slot{
            IsValid(controls_column) ? Cast<UHorizontalBoxSlot>(controls_column->Slot) : nullptr};
        auto* const levels_slot{
            IsValid(levels_column) ? Cast<UHorizontalBoxSlot>(levels_column->Slot) : nullptr};
        auto* const details_slot{
            IsValid(details_column) ? Cast<UHorizontalBoxSlot>(details_column->Slot) : nullptr};
        auto* const page_slot{IsValid(page) ? Cast<UOverlaySlot>(page->Slot) : nullptr};
        auto const layout_valid{IsValid(controls_slot) && IsValid(levels_slot) &&
                                IsValid(details_slot) && IsValid(page_slot)};
        if (!TestRunner->TestTrue(TEXT("Level selector layout is valid"), layout_valid)) {
            return;
        }
        TestRunner->TestTrue(TEXT("Controls column is auto sized"),
                             controls_slot->GetSize().SizeRule == ESlateSizeRule::Automatic);
        TestRunner->TestTrue(TEXT("Levels column is auto sized"),
                             levels_slot->GetSize().SizeRule == ESlateSizeRule::Automatic);
        TestRunner->TestTrue(TEXT("Details column fills remaining width"),
                             details_slot->GetSize().SizeRule == ESlateSizeRule::Fill);
        TestRunner->TestTrue(TEXT("Selector fills the root horizontally"),
                             page_slot->GetHorizontalAlignment() == HAlign_Fill);
        TestRunner->TestTrue(TEXT("Selector fills the root vertically"),
                             page_slot->GetVerticalAlignment() == VAlign_Fill);

        level_button->OnClicked().Broadcast();
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
        TestRunner->TestTrue(TEXT("Selected level reports that it is ready"),
                             status_text->GetText().ToString().Contains(TEXT("Ready to launch.")));
        TestRunner->TestTrue(
            TEXT("Selected level details include its stable id"),
            details_text->GetText().ToString().Contains(TEXT("Level ID: border-skirmish")));

        level_select_widget->ActivateWidget();
        level_back_button->OnClicked().Broadcast();
        TestRunner->TestFalse(TEXT("Level select Back deactivates the screen"),
                              level_select_widget->IsActivated());

        save_games_button->OnClicked().Broadcast();
        TestRunner->TestTrue(TEXT("Save Games opens save viewer"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::SaveGames);

        save_games_back_button->OnClicked.Broadcast();
        TestRunner->TestTrue(TEXT("Save Games Back returns to main"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Main);

        options_button->OnClicked().Broadcast();
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
