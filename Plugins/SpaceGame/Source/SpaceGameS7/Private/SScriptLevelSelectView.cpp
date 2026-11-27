#include "SScriptLevelSelectView.h"

#include <ioj/levels/launch_options.h>
#include <SpaceGame/ui/common/HiveWidgets.h>
#include <SpaceGame/ui/common/SGameButton.h>
#include <SpaceGame/ui/common/SMenuEditableText.h>
#include <SpaceGamePresentation/ui/style/GameUiStyle.h>

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
    on_level_selected_ = args._OnLevelSelected;
    on_category_selected_ = args._OnCategorySelected;
    on_battle_speed_changed_ = args._OnBattleSpeedChanged;
    on_battle_duration_changed_ = args._OnBattleDurationChanged;
    on_battle_simulation_only_changed_ = args._OnBattleSimulationOnlyChanged;
    on_launch_ = args._OnLaunch;

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
        auto const speed_text{
            state.playerless_time_scale.IsSet()
                ? FText::FromString(FString::SanitizeFloat(state.playerless_time_scale.GetValue()))
                : FText::GetEmpty()};
        if (!battle_speed_input_->GetText().EqualTo(speed_text)) {
            battle_speed_input_->SetText(speed_text);
        }

        handle_battle_duration_changed(battle_duration_input_->GetText());
    }

    battle_options_->SetVisibility(playerless ? EVisibility::Visible : EVisibility::Collapsed);
    battle_speed_error_->SetVisibility(playerless && !battle_speed_valid_
                                           ? EVisibility::HitTestInvisible
                                           : EVisibility::Collapsed);
    battle_duration_error_->SetVisibility(playerless && !battle_duration_valid_
                                              ? EVisibility::HitTestInvisible
                                              : EVisibility::Collapsed);
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
    auto const activate{update.operation == MenuOperation::Activate};
    if (update.target.kind == MenuTarget::Kind::Category) {
        auto const category{update.target.category()};
        if (activate) {
            on_category_selected_.ExecuteIfBound(category);
        }
        auto const button{category == LevelCatalogCategory::Playable ? level_category_button_
                          : category == LevelCatalogCategory::BattleViewer
                              ? battle_viewer_category_button_
                              : benchmark_category_button_};
        button->focus();
    } else if (update.target.is_row()) {
        auto const* row{&update.target};
        auto const index{level_ids_.Find(row->id)};
        if (index != INDEX_NONE) {
            if (activate) {
                on_level_selected_.ExecuteIfBound(index);
            }
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
                if (activate) {
                    auto const state{battle_simulation_only_ ? ECheckBoxState::Unchecked
                                                             : ECheckBoxState::Checked};
                    simulation_only_->SetIsChecked(state);
                    handle_simulation_only_changed(state);
                }
                slate.SetKeyboardFocus(simulation_only_);
                break;
            case MenuControl::Launch:
                if (activate) {
                    on_launch_.ExecuteIfBound();
                } else {
                    launch_button_->focus();
                }
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
    auto value_text{text.ToString()};
    value_text.TrimStartAndEndInline();
    auto const numeric{value_text.IsNumeric()};
    auto const value{numeric ? FCString::Atod(*value_text) : 0.0};
    battle_speed_valid_ = numeric && levels::launch::TimeScale::valid(value);
    battle_speed_error_->SetVisibility(battle_speed_valid_ ? EVisibility::Collapsed
                                                           : EVisibility::HitTestInvisible);
    update_launch_availability();
    on_battle_speed_changed_.ExecuteIfBound(battle_speed_valid_ ? TOptional<double>{value}
                                                                : TOptional<double>{});
}

void SScriptLevelSelectView::handle_battle_duration_changed(FText const& text) {
    auto value_text{text.ToString()};
    value_text.TrimStartAndEndInline();
    auto const blank{value_text.IsEmpty()};
    auto const numeric{value_text.IsNumeric()};
    auto const value{numeric ? FCString::Atod(*value_text) : 0.0};
    auto const valid_duration{numeric && PositiveFinite<double>::valid(value)};
    battle_duration_valid_ = blank || valid_duration;
    battle_duration_error_->SetVisibility(battle_duration_valid_ ? EVisibility::Collapsed
                                                                 : EVisibility::HitTestInvisible);
    update_launch_availability();
    on_battle_duration_changed_.ExecuteIfBound(
        valid_duration ? TOptional<double>{value} : TOptional<double>{}, battle_duration_valid_);
}

void SScriptLevelSelectView::handle_simulation_only_changed(ECheckBoxState const state) {
    battle_simulation_only_ = state == ECheckBoxState::Checked;
    handle_battle_duration_changed(battle_duration_input_->GetText());
    on_battle_simulation_only_changed_.ExecuteIfBound(battle_simulation_only_);
}

void SScriptLevelSelectView::update_launch_availability() {
    auto const speed_is_valid{category_ == LevelCatalogCategory::Playable ||
                              (battle_speed_valid_ && battle_duration_valid_)};
    player_menu().launch_options_valid = speed_is_valid;
    launch_button_->SetEnabled(selected_level_can_launch_ && speed_is_valid);
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
