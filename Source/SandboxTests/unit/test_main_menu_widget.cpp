#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/ui/main_menu/MainMenuLandingWidget.h>
#include <SpaceGame/ui/main_menu/MainMenuWidget.h>
#include <SpaceGame/ui/main_menu/OptionsWidget.h>
#include <SpaceGame/ui/save_game/SaveGameViewerWidget.h>
#include <SpaceGame/ui/style/SpaceGameUiSettings.h>
#include <SpaceGame/ui/style/SpaceGameUiTheme.h>
#include <SpaceGameS7/ScriptLevelSelectWidget.h>

#include <CommonInputSettings.h>
#include <Components/Button.h>
#include <CQTest.h>
#include <Engine/Engine.h>
#include <Engine/GameInstance.h>
#include <Engine/World.h>
#include <HAL/FileManager.h>
#include <Layout/ArrangedChildren.h>
#include <Layout/Geometry.h>

namespace {

class FScopedEditorWorldGameInstance {
  public:
    explicit FScopedEditorWorldGameInstance(UWorld& world)
        : world_{&world}
        , previous_{world.GetGameInstance()} {
        if (IsValid(previous_)) {
            return;
        }
        owned_ = NewObject<UGameInstance>(GEngine);
        if (!IsValid(owned_)) {
            return;
        }
        owned_->Init();
        world_->SetGameInstance(owned_);
    }

    ~FScopedEditorWorldGameInstance() {
        if (!IsValid(owned_)) {
            return;
        }
        world_->SetGameInstance(previous_);
        owned_->Shutdown();
    }

    [[nodiscard]] auto is_valid() const -> bool { return IsValid(world_->GetGameInstance()); }
  private:
    UWorld* world_{};
    UGameInstance* previous_{};
    UGameInstance* owned_{};
};

auto find_slate_descendant(TSharedRef<SWidget> const& widget, FName const type)
    -> TSharedPtr<SWidget> {
    if (widget->GetType() == type) {
        return widget;
    }

    auto* const children{widget->GetChildren()};
    auto const child_count{children->Num()};
    for (int32 child_index{}; child_index < child_count; ++child_index) {
        if (auto const result{find_slate_descendant(children->GetChildAt(child_index), type)};
            result.IsValid()) {
            return result;
        }
    }
    return {};
}

auto find_arranged_size(TSharedRef<SWidget> const& widget,
                        FGeometry const& geometry,
                        TSharedRef<SWidget> const& target) -> TOptional<FVector2f> {
    if (widget == target) {
        return FVector2f{geometry.GetAbsoluteSize()};
    }

    FArrangedChildren children{EVisibility::Visible};
    widget->ArrangeChildren(geometry, children);
    auto const child_count{children.Num()};
    for (int32 child_index{}; child_index < child_count; ++child_index) {
        auto const& child{children[child_index]};
        if (auto const result{find_arranged_size(child.Widget, child.Geometry, target)};
            result.IsSet()) {
            return result;
        }
    }
    return {};
}

auto arranged_size(TSharedRef<SWidget> const& root,
                   TSharedRef<SWidget> const& descendant,
                   FVector2D const available_size) -> TOptional<FVector2f> {
    root->SlatePrepass();
    return find_arranged_size(
        root, FGeometry::MakeRoot(available_size, FSlateLayoutTransform{}), descendant);
}

} // namespace

TEST_CLASS(MainMenuWidget, "Sandbox.UnitTests")
{
    TEST_METHOD(Navigation)
    {
        auto const* const theme{GetDefault<ml::ioj::USpaceGameUiTheme>()};
        auto const ui_style{theme->compile()};
        auto const& text_style{ui_style.text(EGameTextStyle::Body)};
        TestRunner->TestTrue(TEXT("Common menu text style has a renderable font"),
                             text_style.Font.CompositeFont.IsValid() ||
                                 IsValid(text_style.Font.FontObject));
        auto const& palette{ui_style.palette()};
        TestRunner->TestTrue(TEXT("Hive canvas remains dark"),
                             palette.canvas.GetLuminance() < 0.02f);
        TestRunner->TestTrue(TEXT("Honey is brighter than the raised surface"),
                             palette.honey.GetLuminance() > palette.surface_raised.GetLuminance());
        TestRunner->TestTrue(TEXT("Primary text remains readable against the canvas"),
                             palette.text_primary.GetLuminance() - palette.canvas.GetLuminance() >
                                 0.65f);
        for (int32 index{}; index < TEnumTraits<EGameUiIcon>::count; ++index) {
            auto const icon_role{static_cast<EGameUiIcon>(index)};
            auto const& icon{ui_style.icon(icon_role)};
            TestRunner->TestTrue(
                *FString::Printf(TEXT("%s icon is vector-backed"), LexToString(icon_role)),
                icon.ImageType == ESlateBrushImageType::Vector);
            auto const resource_path{icon.GetResourceName().ToString()};
            TestRunner->TestTrue(
                *FString::Printf(TEXT("%s icon resource exists"), LexToString(icon_role)),
                IFileManager::Get().FileExists(*resource_path));
        }

        auto const* const ui_settings{GetDefault<ml::ioj::USpaceGameUiSettings>()};
        auto* const configured_theme{ui_settings->default_theme.LoadSynchronous()};
        if (!TestRunner->TestTrue(TEXT("Configured UI theme loads"), IsValid(configured_theme))) {
            return;
        }
        auto const configured_style{configured_theme->compile()};
        auto const panel_color{configured_style.panel().background.TintColor.GetSpecifiedColor()};
        TestRunner->TestTrue(TEXT("Configured panel background is dark"),
                             panel_color.GetLuminance() < 0.25f);
        TestRunner->TestTrue(TEXT("Configured panel background remains translucent"),
                             panel_color.A > 0.0f && panel_color.A < 0.95f);

        auto const world_result{ml::get_editor_world()};
        if (!TestRunner->TestTrue(TEXT("Editor world is available"), world_result.has_value())) {
            return;
        }
        FScopedEditorWorldGameInstance game_instance{*world_result.value()};
        if (!TestRunner->TestTrue(TEXT("Test game instance is available"),
                                  game_instance.is_valid())) {
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

        auto* const main_page{
            Cast<ml::ioj::UMainMenuLandingWidget>(widget->GetWidgetFromName(TEXT("main_page")))};
        auto* const options_widget{
            Cast<ml::ioj::UOptionsWidget>(widget->GetWidgetFromName(TEXT("options_widget")))};
        auto* const save_game_viewer{Cast<ml::ioj::USaveGameViewerWidget>(
            widget->GetWidgetFromName(TEXT("save_game_viewer")))};
        auto* const save_games_back_button{
            Cast<UButton>(widget->GetWidgetFromName(TEXT("save_games_back_button")))};

        auto const main_bindings_valid{IsValid(main_page) && IsValid(save_game_viewer) &&
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

        auto const level_select_frame{
            find_slate_descendant(level_select_slate, FName{TEXT("ml::ioj::SHiveFrame")})};
        if (!TestRunner->TestTrue(TEXT("Level selector uses the reusable Hive frame"),
                                  level_select_frame.IsValid())) {
            return;
        }

        TestRunner->TestTrue(TEXT("Main page is active initially"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Main);
        TestRunner->TestTrue(TEXT("Landing page is the deterministic initial focus target"),
                             widget->GetDesiredFocusTarget() == main_page);

        bool level_select_requested{false};
        widget->level_select_requested.AddLambda(
            [&level_select_requested] { level_select_requested = true; });
        main_page->select_mission_requested.Broadcast();
        TestRunner->TestTrue(TEXT("Select Mission requests level select"), level_select_requested);
        TestRunner->TestEqual(TEXT("Preferred stable level id restores selection"),
                              level_select_widget->get_selected_level_id(),
                              FName{TEXT("turret-trial-0")});
        TestRunner->TestTrue(TEXT("Preferred valid level can be launched immediately"),
                             level_select_widget->can_launch_selected_level());
        TestRunner->TestTrue(TEXT("Level selector is the CommonUI focus bridge"),
                             level_select_widget->GetDesiredFocusTarget() == level_select_widget);

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

        auto const selector_available_size{FVector2D{1366.0, 768.0}};
        auto const selector_frame_size{arranged_size(
            level_select_slate, level_select_frame.ToSharedRef(), selector_available_size)};
        if (!TestRunner->TestTrue(TEXT("Level selector frame is arranged"),
                                  selector_frame_size.IsSet())) {
            return;
        }
        TestRunner->TestTrue(
            TEXT("Level selector fills the available viewport"),
            selector_frame_size.GetValue().Equals(FVector2f{selector_available_size}));
        main_page->save_data_requested.Broadcast();
        TestRunner->TestTrue(TEXT("Save Data opens save viewer"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::SaveGames);

        save_games_back_button->OnClicked.Broadcast();
        TestRunner->TestTrue(TEXT("Save Games Back returns to main"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Main);

        main_page->options_requested.Broadcast();
        TestRunner->TestTrue(TEXT("Options opens options page"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Options);
        TestRunner->TestTrue(TEXT("Video is the initial options tab"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::Video);

        auto const options_slate{options_widget->TakeWidget()};
        auto const options_frame{
            find_slate_descendant(options_slate, FName{TEXT("::ml::ioj::SHiveFrame")})};
        if (!TestRunner->TestTrue(TEXT("Options contains its reusable Hive frame"),
                                  options_frame.IsValid())) {
            return;
        }
        auto const window_size{configured_style.settings().window_size};
        options_slate->SlatePrepass();
        auto const video_desired_size{FVector2f{options_frame->GetDesiredSize()}};
        TestRunner->TestTrue(TEXT("Populated options frame reports its configured size"),
                             video_desired_size.Equals(window_size));

        auto const constrained_size{FVector2D{480.0, 594.0}};
        auto const video_frame_size{
            arranged_size(options_slate, options_frame.ToSharedRef(), constrained_size)};
        if (!TestRunner->TestTrue(TEXT("Options frame is arranged in a constrained viewport"),
                                  video_frame_size.IsSet())) {
            return;
        }

        options_widget->select_tab(ml::ioj::EOptionsTab::Gameplay);
        TestRunner->TestTrue(TEXT("Gameplay tab is selectable"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::Gameplay);

        options_widget->select_tab(ml::ioj::EOptionsTab::Audio);
        TestRunner->TestTrue(TEXT("Audio tab is selectable"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::Audio);

        options_widget->select_tab(ml::ioj::EOptionsTab::Controls);
        TestRunner->TestTrue(TEXT("Controls tab is selectable"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::Controls);

        options_widget->select_tab(ml::ioj::EOptionsTab::Accessibility);
        TestRunner->TestTrue(TEXT("Accessibility tab is selectable"),
                             options_widget->get_active_tab() ==
                                 ml::ioj::EOptionsTab::Accessibility);
        options_slate->SlatePrepass();
        auto const accessibility_desired_size{FVector2f{options_frame->GetDesiredSize()}};
        TestRunner->TestTrue(TEXT("Sparse options frame reports its configured size"),
                             accessibility_desired_size.Equals(window_size));
        auto const accessibility_frame_size{
            arranged_size(options_slate, options_frame.ToSharedRef(), constrained_size)};
        if (!TestRunner->TestTrue(TEXT("Sparse options frame remains arranged"),
                                  accessibility_frame_size.IsSet())) {
            return;
        }
        TestRunner->TestTrue(
            TEXT("Options keeps stable arranged dimensions across sparse and populated tabs"),
            video_frame_size.GetValue().Equals(accessibility_frame_size.GetValue()));
        TestRunner->TestTrue(TEXT("Constrained options fit within the available viewport"),
                             accessibility_frame_size.GetValue().X <= constrained_size.X &&
                                 accessibility_frame_size.GetValue().Y <= constrained_size.Y);

        options_widget->select_tab(ml::ioj::EOptionsTab::System);
        TestRunner->TestTrue(TEXT("System tab is selectable"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::System);

        options_widget->select_tab(ml::ioj::EOptionsTab::Video);
        TestRunner->TestTrue(TEXT("Video tab is selectable"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::Video);

        TestRunner->TestTrue(TEXT("Options is the CommonUI focus bridge"),
                             options_widget->get_focus_target() == options_widget);
        options_widget->request_back();
        TestRunner->TestTrue(TEXT("Options Back returns to main"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Main);
    }
};
