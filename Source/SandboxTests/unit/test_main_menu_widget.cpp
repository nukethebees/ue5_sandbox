#include <SandboxTests/support/test_setup.h>

#include <SpaceGame/ui/main_menu/DebugSettingsWidget.h>
#include <SpaceGame/ui/main_menu/MainMenuWidget.h>
#include <SpaceGame/ui/main_menu/OptionsWidget.h>
#include <SpaceGame/ui/save_game/SaveGameViewerWidget.h>
#include <SpaceGame/ui/style/SpaceGameUiSettings.h>
#include <SpaceGame/ui/style/SpaceGameUiTheme.h>
#include <SpaceGame/ui/telemetry/TelemetryDashboardWidget.h>
#include <SpaceGameS7/ScriptLevelSelectWidget.h>

#include <CommonInputSettings.h>
#include <CQTest.h>
#include <Engine/Engine.h>
#include <Engine/GameInstance.h>
#include <Engine/World.h>
#include <HAL/FileManager.h>
#include <Layout/ArrangedChildren.h>
#include <Layout/Geometry.h>
#include <Widgets/Layout/SWidgetSwitcher.h>

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

        auto const level_select_class{LoadClass<ml::s7::UScriptLevelSelectWidget>(
            nullptr, TEXT("/SpaceGame/UI/MainMenu/WBP_LevelSelect.WBP_LevelSelect_C"))};
        if (!TestRunner->TestTrue(TEXT("Level selector class loads"),
                                  IsValid(level_select_class))) {
            return;
        }
        widget->prepare_for_open(level_select_class, false, TEXT("turret-trial-0"));
        FCommonInputBase::GetInputSettings()->LoadData();
        auto const slate_widget{widget->TakeWidget()};

        auto* const level_select_widget{
            Cast<ml::s7::UScriptLevelSelectWidget>(widget->get_level_select_widget())};
        auto* const options_widget{widget->get_options_widget()};
        auto* const debug_settings_widget{widget->get_debug_settings_widget()};
        auto* const save_game_viewer{widget->get_save_game_viewer()};
        auto* const telemetry_dashboard{widget->get_telemetry_dashboard()};
        if (!TestRunner->TestTrue(TEXT("All command-deck pages are created"),
                                  IsValid(level_select_widget) && IsValid(options_widget) &&
                                      IsValid(save_game_viewer) && IsValid(telemetry_dashboard) &&
                                      IsValid(debug_settings_widget))) {
            return;
        }

        TestRunner->TestTrue(TEXT("Select Mission is active initially"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::SelectMission);
        TestRunner->TestTrue(TEXT("Mission browser defaults to Missions"),
                             level_select_widget->get_active_category() ==
                                 ml::s7::ELevelCatalogCategory::Mission);
        TestRunner->TestTrue(TEXT("The command deck is the CommonUI focus bridge"),
                             widget->GetDesiredFocusTarget() == widget);
        TestRunner->TestEqual(TEXT("Preferred stable level id restores selection"),
                              level_select_widget->get_selected_level_id(),
                              FName{TEXT("turret-trial-0")});
        TestRunner->TestTrue(TEXT("Preferred valid level can be launched immediately"),
                             level_select_widget->can_launch_selected_level());

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

        auto const shell_frame{find_slate_descendant(slate_widget, FName{TEXT("SHiveFrame")})};
        if (!TestRunner->TestTrue(TEXT("Command deck uses one reusable Hive frame"),
                                  shell_frame.IsValid())) {
            return;
        }
        auto const available_size{FVector2D{1366.0, 768.0}};
        auto const shell_size{
            arranged_size(slate_widget, shell_frame.ToSharedRef(), available_size)};
        TestRunner->TestTrue(TEXT("Command deck fills the available viewport"),
                             shell_size.IsSet() &&
                                 shell_size.GetValue().Equals(FVector2f{available_size}));

        widget->select_page(ml::ioj::EMainMenuPage::DataArchive);
        TestRunner->TestTrue(TEXT("Data Archive is selectable"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::DataArchive);

        widget->select_page(ml::ioj::EMainMenuPage::Telemetry);
        TestRunner->TestTrue(TEXT("Telemetry is selectable"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Telemetry);

        widget->select_page(ml::ioj::EMainMenuPage::Video);
        TestRunner->TestTrue(TEXT("Video configuration is selectable"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Video);
        TestRunner->TestTrue(TEXT("Video is the initial options tab"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::Video);
        widget->select_page(ml::ioj::EMainMenuPage::Gameplay);
        TestRunner->TestTrue(TEXT("Gameplay tab is selectable"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::Gameplay);
        widget->select_page(ml::ioj::EMainMenuPage::Audio);
        TestRunner->TestTrue(TEXT("Audio tab is selectable"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::Audio);
        widget->select_page(ml::ioj::EMainMenuPage::Controls);
        TestRunner->TestTrue(TEXT("Controls tab is selectable"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::Controls);
        widget->select_page(ml::ioj::EMainMenuPage::Accessibility);
        TestRunner->TestTrue(TEXT("Accessibility tab is selectable"),
                             options_widget->get_active_tab() ==
                                 ml::ioj::EOptionsTab::Accessibility);
        widget->select_page(ml::ioj::EMainMenuPage::System);
        TestRunner->TestTrue(TEXT("System tab is selectable"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::System);

        widget->select_page(ml::ioj::EMainMenuPage::Debug);
        TestRunner->TestTrue(TEXT("Debug page is selectable"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Debug);
        TestRunner->TestTrue(TEXT("Debug remains outside the Options tab model"),
                             options_widget->get_active_tab() == ml::ioj::EOptionsTab::System);

        widget->select_page(ml::ioj::EMainMenuPage::SelectMission);
        TestRunner->TestTrue(TEXT("Operations remain reachable after configuration"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::SelectMission);

        auto const switcher_widget{
            find_slate_descendant(slate_widget, FName{TEXT("SWidgetSwitcher")})};
        if (!TestRunner->TestTrue(TEXT("Command deck has a page switcher"),
                                  switcher_widget.IsValid())) {
            return;
        }
        auto const switcher{StaticCastSharedPtr<SWidgetSwitcher>(switcher_widget)};
        widget->prepare_for_open(level_select_class, false, NAME_None, true);
        TestRunner->TestTrue(TEXT("A post-construction telemetry request updates the active page"),
                             widget->get_active_page() == ml::ioj::EMainMenuPage::Telemetry);
        TestRunner->TestEqual(
            TEXT("A post-construction telemetry request updates the visible page"),
            switcher->GetActiveWidgetIndex(),
            int32{2});
    }
};
