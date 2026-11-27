#include "SScriptLevelSelectView.h"

#include <ioj/levels/launch_options.h>
#include <SpaceGame/ui/common/HiveWidgets.h>
#include <SpaceGame/ui/common/SGameButton.h>
#include <SpaceGame/ui/common/SMenuEditableText.h>
#include <SpaceGamePresentation/ui/style/GameUiStyle.h>

#include <SandboxCoreEngine/strings.h>
#include <SandboxUI/slate/SlateSlots.h>

#include <Framework/Application/SlateApplication.h>
#include <Widgets/Input/SCheckBox.h>
#include <Widgets/Input/SEditableText.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Text/STextBlock.h>

#include "generated/ioj/s7/SScriptLevelSelectView.slate.generated.h"

namespace ioj::s7 {
using ioj::levels::authoring::LevelCatalogCategory;

void SScriptLevelSelectView::Construct(FArguments const& args) {
    style_ = args._Style;
    check(style_ != nullptr);

    ChildSlot[SlateGenerated::ioj::s7::SScriptLevelSelectViewBuilder{*this}.BuildRoot(
        &style_->chrome().body_background, build_header(), build_catalog(), build_details())];
}

void SScriptLevelSelectView::replace_catalog(FLevelSelectViewState const& state) {
    rebuild_catalog(state);
    update_state(state);
}

void SScriptLevelSelectView::update_state(FLevelSelectViewState const& state) {
    category_ = state.category;
    selected_button_index_ = state.selected_button_index;
    update_category_selection();
    update_selection(selected_button_index_);

    title_->SetText(state.title);
    status_->SetText(state.status);
    description_->SetText(state.description);
    filename_->SetText(state.filename);
    details_->SetText(state.details);
    script_->SetText(state.script);
    launch_mode_status_->SetText(state.launch_mode_status);
    launch_mode_status_->SetVisibility(state.launch_mode_status.IsEmpty()
                                           ? EVisibility::Collapsed
                                           : EVisibility::HitTestInvisible);
    selected_level_can_launch_ = state.can_launch;
    auto const playerless{category_ != LevelCatalogCategory::Playable};
    if (playerless) {
        auto const speed_text{FText::FromString(to_fstring(player_menu().launch_speed()))};
        if (!battle_speed_input_->GetText().EqualTo(speed_text)) {
            battle_speed_input_->SetText(speed_text);
        }

        auto const duration{FText::FromString(to_fstring(player_menu().duration))};
        if (!battle_duration_input_->GetText().EqualTo(duration)) {
            battle_duration_input_->SetText(duration);
        }
        simulation_only_->SetIsChecked(player_menu().simulation_only
                                           ? ECheckBoxState::Checked
                                           : ECheckBoxState::Unchecked);
    }

    battle_options_->SetVisibility(playerless ? EVisibility::Visible : EVisibility::Collapsed);
    update_launch_availability();
    Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

auto SScriptLevelSelectView::OnFocusReceived(FGeometry const& geometry,
                                             FFocusEvent const& focus_event) -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    present({player_menu().focus, MenuOperation::Focus});
    return FReply::Handled();
}

void SScriptLevelSelectView::present(MenuUpdate const& update) {
    if (update.operation == MenuOperation::RefreshSettings) {
        update_launch_availability();
        return;
    }
    if (update.target.kind == MenuTarget::Kind::Category) {
        auto const category{update.target.category()};
        auto const button{category == LevelCatalogCategory::Playable ? level_category_button_
                          : category == LevelCatalogCategory::BattleViewer
                              ? battle_viewer_category_button_
                              : benchmark_category_button_};
        button->focus();
    } else if (update.target.is_row()) {
        auto const index{level_ids_.Find(update.target.id)};
        if (index != INDEX_NONE) {
            focus_level(index);
        }
    } else if (update.target.kind == MenuTarget::Kind::Control) {
        auto const control{update.target.control()};
        auto& slate{FSlateApplication::Get()};
        switch (control) {
            case MenuControl::BattleSpeed:
                slate.SetKeyboardFocus(battle_speed_input_);
                break;
            case MenuControl::BattleDuration:
                slate.SetKeyboardFocus(battle_duration_input_);
                break;
            case MenuControl::SimulationOnly:
                simulation_only_->SetIsChecked(player_menu().simulation_only
                                                   ? ECheckBoxState::Checked
                                                   : ECheckBoxState::Unchecked);
                slate.SetKeyboardFocus(simulation_only_);
                break;
            case MenuControl::Launch:
                launch_button_->focus();
                break;
            default:
                break;
        }
    }
}

auto SScriptLevelSelectView::build_header() -> TSharedRef<SWidget> {
    auto const playable_level_button{
        SAssignNew(level_category_button_, SGameButton)
            .Style(&style_->button(EGameButtonStyle::Secondary))
            .Text(NSLOCTEXT("LevelSelect", "Missions", "MISSIONS"))
            .Selected(true)
            .OnClicked(this,
                       &SScriptLevelSelectView::handle_category_selected,
                       LevelCatalogCategory::Playable)};
    auto const battle_viewer_button{
        SAssignNew(battle_viewer_category_button_, SGameButton)
            .Style(&style_->button(EGameButtonStyle::Secondary))
            .Text(NSLOCTEXT("LevelSelect", "BattleViewer", "BATTLE VIEWER"))
            .OnClicked(this,
                       &SScriptLevelSelectView::handle_category_selected,
                       LevelCatalogCategory::BattleViewer)};
    auto const benchmark_button{SAssignNew(benchmark_category_button_, SGameButton)
                                    .Style(&style_->button(EGameButtonStyle::Secondary))
                                    .Text(NSLOCTEXT("LevelSelect", "Benchmarks", "BENCHMARKS"))
                                    .OnClicked(this,
                                               &SScriptLevelSelectView::handle_category_selected,
                                               LevelCatalogCategory::Benchmark)};

    return SlateGenerated::ioj::s7::SScriptLevelSelectViewBuilder{*this}.BuildHeader(
        &style_->chrome().frame_border,
        &style_->text(EGameTextStyle::Caption),
        &style_->text(EGameTextStyle::Heading3),
        playable_level_button,
        battle_viewer_button,
        benchmark_button);
}

auto SScriptLevelSelectView::build_catalog() -> TSharedRef<SWidget> {
    auto const catalog_scroll{SNew(SScrollBox).ScrollBarStyle(&style_->chrome().scroll_bar) +
                              SScrollBox::Slot()[SAssignNew(catalog_rows_, SVerticalBox)]};
    return SlateGenerated::ioj::s7::SScriptLevelSelectViewBuilder{*this}.BuildCatalog(
        &style_->chrome().navigation_background,
        &style_->text(EGameTextStyle::Caption),
        &style_->text(EGameTextStyle::Heading3),
        catalog_scroll);
}

auto SScriptLevelSelectView::build_details() -> TSharedRef<SWidget> {
    auto const script_panel{
        SNew(SBorder)
            .BorderImage(&style_->chrome().frame_border)
            .Padding(FMargin{1.0f})
                [SNew(SBorder)
                     .BorderImage(&style_->chrome().navigation_background)
                     .Padding(FMargin{16.0f})
                         [SNew(SScrollBox).ScrollBarStyle(&style_->chrome().scroll_bar) +
                          SScrollBox::Slot()[SAssignNew(script_, STextBlock)
                                                 .TextStyle(&style_->text(EGameTextStyle::Caption))
                                                 .AutoWrapText(false)]]]};

    auto const launch{SNew(SBox).WidthOverride(
        180.0f)[SAssignNew(launch_button_, SGameButton)
                    .Style(&style_->button(EGameButtonStyle::Primary))
                    .Text(this, &SScriptLevelSelectView::launch_text)
                    .OnClicked(this, &SScriptLevelSelectView::request_launch)]};
    auto const battle_speed{
        SAssignNew(battle_options_, SVerticalBox) +
        SVerticalBox::Slot().AutoHeight()
            [SNew(SHorizontalBox) +
             SHorizontalBox::Slot()
                 .AutoWidth()
                 .VAlign(VAlign_Center)
                 .Padding(FMargin{0.0f, 0.0f, 10.0f, 0.0f})
                     [SNew(STextBlock)
                          .Text(NSLOCTEXT("LevelSelect", "BattleSpeed", "BATTLE SPEED //"))
                          .TextStyle(&style_->text(EGameTextStyle::Caption))] +
             SHorizontalBox::Slot().AutoWidth()
                 [SNew(SBorder)
                      .BorderImage(&style_->chrome().frame_border)
                      .Padding(FMargin{1.0f})
                          [SNew(SBorder)
                               .BorderImage(&style_->chrome().navigation_background)
                               .Padding(FMargin{10.0f, 6.0f})[SNew(SBox).WidthOverride(
                                   80.0f)[SAssignNew(battle_speed_input_,
                                                     SMenuEditableText,
                                                     MenuControl::BattleSpeed)
                                              .Text(FText::FromString(TEXT("1")))
                                              .Font(style_->text(EGameTextStyle::Body).Font)
                                              .ColorAndOpacity(style_->palette().text_primary)
                                              .SelectAllTextWhenFocused(true)
                                              .RevertTextOnEscape(true)
                                              .OnTextChanged(this,
                                                             &SScriptLevelSelectView::
                                                                 handle_battle_speed_changed)]]]] +
             SHorizontalBox::Slot()
                 .AutoWidth()
                 .VAlign(VAlign_Center)
                 .Padding(FMargin{8.0f, 0.0f, 0.0f, 0.0f})
                     [SNew(STextBlock)
                          .Text(NSLOCTEXT("LevelSelect", "BattleSpeedSuffix", "x"))
                          .TextStyle(&style_->text(EGameTextStyle::Body))]] +
        SVerticalBox::Slot().AutoHeight().Padding(FMargin{
            0.0f,
            6.0f,
            0.0f,
            0.0f})[SAssignNew(battle_speed_error_, STextBlock)
                       .Text(NSLOCTEXT("LevelSelect",
                                       "InvalidBattleSpeed",
                                       "Enter a battle speed greater than 0 and at most 100."))
                       .TextStyle(&style_->text(EGameTextStyle::Warning))
                       .Visibility(EVisibility::Collapsed)] +
        SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 10.0f, 0.0f, 0.0f})
            [SNew(SHorizontalBox) +
             SHorizontalBox::Slot().AutoWidth().VAlign(
                 VAlign_Center)[SNew(STextBlock)
                                    .Text(NSLOCTEXT("LevelSelect", "BattleDuration", "DURATION //"))
                                    .TextStyle(&style_->text(EGameTextStyle::Caption))] +
             SHorizontalBox::Slot().AutoWidth().Padding(
                 FMargin{10.0f, 0.0f})[SNew(SBox).WidthOverride(
                 80.0f)[SAssignNew(
                            battle_duration_input_, SMenuEditableText, MenuControl::BattleDuration)
                            .HintText(NSLOCTEXT("LevelSelect", "UnlimitedDuration", "Unlimited"))
                            .Font(style_->text(EGameTextStyle::Body).Font)
                            .ColorAndOpacity(style_->palette().text_primary)
                            .SelectAllTextWhenFocused(true)
                            .RevertTextOnEscape(true)
                            .OnTextChanged(
                                this, &SScriptLevelSelectView::handle_battle_duration_changed)]] +
             SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                 [SNew(STextBlock)
                      .Text(NSLOCTEXT("LevelSelect", "BattleDurationSuffix", "simulated seconds"))
                      .TextStyle(&style_->text(EGameTextStyle::Body))]] +
        SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 6.0f, 0.0f, 0.0f})
            [SAssignNew(battle_duration_error_, STextBlock)
                 .Text(NSLOCTEXT("LevelSelect",
                                 "InvalidBattleDuration",
                                 "Enter a positive duration, or leave blank for no time limit."))
                 .TextStyle(&style_->text(EGameTextStyle::Warning))
                 .Visibility(EVisibility::Collapsed)] +
        SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 10.0f, 0.0f, 0.0f})
            [SNew(SHorizontalBox) +
             SHorizontalBox::Slot().AutoWidth()
                 [SAssignNew(simulation_only_, SCheckBox)
                      .OnCheckStateChanged(this,
                                           &SScriptLevelSelectView::handle_simulation_only_changed)
                          [SNew(STextBlock)
                               .Text(NSLOCTEXT("LevelSelect", "SimulationOnly", "Simulation only"))
                               .TextStyle(&style_->text(EGameTextStyle::Body))]]]};

    return SNew(SBorder)
        .BorderImage(&style_->chrome().body_background)
        .Padding(FMargin{30.0f})
            [SNew(SVerticalBox) +
             SVerticalBox::Slot()
                 .AutoHeight()[SAssignNew(filename_, STextBlock)
                                   .TextStyle(&style_->text(EGameTextStyle::Caption))] +
             SVerticalBox::Slot().AutoHeight().Padding(FMargin{
                 0.0f, 8.0f, 0.0f, 0.0f})[SAssignNew(title_, STextBlock)
                                              .TextStyle(&style_->text(EGameTextStyle::Heading1))
                                              .AutoWrapText(true)] +
             SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 8.0f, 0.0f, 18.0f})
                 [SAssignNew(status_, STextBlock)
                      .TextStyle(&style_->text(EGameTextStyle::BodySecondary))
                      .AutoWrapText(true)] +
             SVerticalBox::Slot().AutoHeight()[SAssignNew(description_, STextBlock)
                                                   .TextStyle(&style_->text(EGameTextStyle::Body))
                                                   .AutoWrapText(true)] +
             SVerticalBox::Slot().AutoHeight().Padding(
                 FMargin{0.0f, 18.0f})[SAssignNew(details_, STextBlock)
                                           .TextStyle(&style_->text(EGameTextStyle::BodySecondary))
                                           .AutoWrapText(true)] +
             SVerticalBox::Slot().AutoHeight().Padding(
                 FMargin{0.0f, 8.0f})[SNew(SHiveSectionHeader)
                                          .Style(style_)
                                          .Text(this, &SScriptLevelSelectView::directive_title)] +
             SVerticalBox::Slot().FillHeight(1.0f)[script_panel] +
             SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 18.0f, 0.0f, 0.0f})
                 [SNew(SHorizontalBox) +
                  SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[battle_speed] +
                  SHorizontalBox::Slot()
                      .AutoWidth()
                      .Padding(FMargin{0.0f, 0.0f, 12.0f, 0.0f})
                      .VAlign(VAlign_Center)[SAssignNew(launch_mode_status_, STextBlock)
                                                 .TextStyle(&style_->text(EGameTextStyle::Caption))
                                                 .Visibility(EVisibility::Collapsed)] +
                  SHorizontalBox::Slot().AutoWidth()[launch]]];
}

auto SScriptLevelSelectView::catalog_caption() const -> FText {
    switch (category_) {
        case LevelCatalogCategory::Playable:
            return NSLOCTEXT("LevelSelect", "MissionCatalogCaption", "MISSION ARCHIVE");
        case LevelCatalogCategory::BattleViewer:
            return NSLOCTEXT("LevelSelect", "BattleCatalogCaption", "BATTLE ARCHIVE");
        case LevelCatalogCategory::Benchmark:
            return NSLOCTEXT("LevelSelect", "BenchmarkCatalogCaption", "BENCHMARK ARCHIVE");
    }

    return FText::GetEmpty();
}

auto SScriptLevelSelectView::catalog_title() const -> FText {
    return category_ == LevelCatalogCategory::Playable
             ? NSLOCTEXT("LevelSelect", "MissionCatalogTitle", "OPERATIONS")
             : NSLOCTEXT("LevelSelect", "BattleCatalogTitle", "SCENARIOS");
}

auto SScriptLevelSelectView::directive_title() const -> FText {
    return category_ == LevelCatalogCategory::Playable
             ? NSLOCTEXT("LevelSelect", "MissionDirective", "MISSION DIRECTIVE")
             : NSLOCTEXT("LevelSelect", "ScenarioDefinition", "SCENARIO DEFINITION");
}

auto SScriptLevelSelectView::launch_text() const -> FText {
    switch (category_) {
        case LevelCatalogCategory::Playable:
            return NSLOCTEXT("LevelSelect", "Launch", "Launch Mission");
        case LevelCatalogCategory::BattleViewer:
            return NSLOCTEXT("LevelSelect", "ViewBattle", "View Battle");
        case LevelCatalogCategory::Benchmark:
            return NSLOCTEXT("LevelSelect", "RunBenchmark", "Run Benchmark");
    }

    return FText::GetEmpty();
}

auto SScriptLevelSelectView::handle_category_selected(LevelCatalogCategory const category)
    -> FReply {
    hit_menu_control(category);
    return FReply::Handled();
}

auto SScriptLevelSelectView::handle_level_selected(int32 const button_index) -> FReply {
    hit_menu_control(MenuTarget{MenuTarget::Kind::Level, level_ids_[button_index]});
    return FReply::Handled();
}

auto SScriptLevelSelectView::request_launch() -> FReply {
    hit_menu_control(MenuControl::Launch);
    return FReply::Handled();
}

void SScriptLevelSelectView::handle_battle_speed_changed(FText const& text) {
    auto value{to_string(text.ToString())};
    if (value != player_menu().launch_speed()) {
        player_menu().edit_text(MenuControl::BattleSpeed, std::move(value));
        update_launch_availability();
    }
}
void SScriptLevelSelectView::handle_battle_duration_changed(FText const& text) {
    auto value{to_string(text.ToString())};
    if (value != player_menu().duration) {
        player_menu().edit_text(MenuControl::BattleDuration, std::move(value));
        update_launch_availability();
    }
}
void SScriptLevelSelectView::handle_simulation_only_changed(ECheckBoxState state) {
    if ((state == ECheckBoxState::Checked) != player_menu().simulation_only) {
        hit_menu_control(MenuControl::SimulationOnly);
    }
}
void SScriptLevelSelectView::update_launch_availability() {
    auto const& menu{player_menu()};
    auto const playerless{category_ != LevelCatalogCategory::Playable};
    battle_speed_error_->SetVisibility(
        playerless && !menu.speed_value() ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
    battle_duration_error_->SetVisibility(playerless && !menu.duration.empty() &&
                                                  !menu.duration_value()
                                              ? EVisibility::HitTestInvisible
                                              : EVisibility::Collapsed);
    launch_button_->SetEnabled(selected_level_can_launch_ && menu.launch_options().has_value());
}

void SScriptLevelSelectView::rebuild_catalog(FLevelSelectViewState const& state) {
    catalog_rows_->ClearChildren();
    level_buttons_.Reset();
    level_ids_.Reset();

    for (auto const& row : state.rows) {
        if (row.heading) {
            catalog_rows_->AddSlot().AutoHeight().Padding(FMargin{4.0f, 14.0f, 4.0f, 6.0f})
                [SNew(STextBlock).Text(row.text).TextStyle(&style_->text(EGameTextStyle::Caption))];
            continue;
        }

        auto const button_index{level_buttons_.Num()};
        auto const completed{row.indicator_state != profiles::CompletionIndicatorState::Incomplete};
        auto const* const icon{completed ? &style_->icon(EGameUiIcon::LevelComplete)
                                         : &style_->icon(EGameUiIcon::LevelIncomplete)};
        auto const icon_tint{row.indicator_state == profiles::CompletionIndicatorState::ParAchieved
                                 ? style_->palette().honey
                             : completed ? style_->palette().completion
                                         : style_->palette().text_secondary};
        TSharedPtr<SGameButton> button;
        catalog_rows_->AddSlot().AutoHeight().Padding(
            FMargin{0.0f, 0.0f, 0.0f, style_->chrome().navigation_spacing})
            [SAssignNew(button, SGameButton)
                 .Style(&style_->button(EGameButtonStyle::Secondary))
                 .Icon(icon)
                 .IconTint(icon_tint)
                 .IconSize(FVector2D{18.0f, 18.0f})
                 .IconSpacing(10.0f)
                 .ContentAlignment(HAlign_Left)
                 .Text(row.text)
                 .Selected(button_index == state.selected_button_index)
                 .OnClicked(this, &SScriptLevelSelectView::handle_level_selected, button_index)];
        level_buttons_.Add(button);
        level_ids_.Add(row.id);
    }
}

void SScriptLevelSelectView::update_selection(int32 const button_index) {
    auto const button_count{level_buttons_.Num()};
    for (int32 index{}; index < button_count; ++index) {
        level_buttons_[index]->set_selected(index == button_index);
    }
}

void SScriptLevelSelectView::update_category_selection() {
    if (level_category_button_.IsValid()) {
        level_category_button_->set_selected(category_ == LevelCatalogCategory::Playable);
    }

    if (battle_viewer_category_button_.IsValid()) {
        battle_viewer_category_button_->set_selected(category_ ==
                                                     LevelCatalogCategory::BattleViewer);
    }

    if (benchmark_category_button_.IsValid()) {
        benchmark_category_button_->set_selected(category_ == LevelCatalogCategory::Benchmark);
    }
}

void SScriptLevelSelectView::focus_level(int32 const button_index) {
    if (level_buttons_.IsValidIndex(button_index) && level_buttons_[button_index].IsValid()) {
        level_buttons_[button_index]->focus();
    }
}
} // namespace ioj::s7
