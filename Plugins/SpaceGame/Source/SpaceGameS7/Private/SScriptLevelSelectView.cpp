#include "SScriptLevelSelectView.h"

#include <SpaceGame/system/GameSubsystem.h>
#include <SpaceGame/ui/common/HiveWidgets.h>
#include <SpaceGame/ui/common/SGameButton.h>

#include <InputCoreTypes.h>
#include <Widgets/Input/SEditableText.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Text/STextBlock.h>

namespace ml::s7 {
void SScriptLevelSelectView::Construct(FArguments const& args) {
    style_ = args._Style;
    check(style_ != nullptr);
    on_level_selected_ = args._OnLevelSelected;
    on_category_selected_ = args._OnCategorySelected;
    on_battle_speed_changed_ = args._OnBattleSpeedChanged;
    on_launch_ = args._OnLaunch;

    auto const body{SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[build_catalog()] +
                    SHorizontalBox::Slot().FillWidth(1.0f).Padding(
                        FMargin{2.0f, 0.0f, 0.0f, 0.0f})[build_details()]};

    ChildSlot[SNew(SBorder)
                  .BorderImage(&style_->chrome().body_background)
                  .Padding(FMargin{24.0f})[SNew(SVerticalBox) +
                                           SVerticalBox::Slot().AutoHeight()[build_header()] +
                                           SVerticalBox::Slot().FillHeight(1.0f).Padding(
                                               FMargin{0.0f, 18.0f, 0.0f, 0.0f})[body]]];
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
    auto const battle_viewer{category_ == ELevelCatalogCategory::BattleViewer};
    battle_speed_control_->SetVisibility(battle_viewer ? EVisibility::Visible
                                                       : EVisibility::Collapsed);
    battle_speed_error_->SetVisibility(battle_viewer && !battle_speed_valid_
                                           ? EVisibility::HitTestInvisible
                                           : EVisibility::Collapsed);
    update_launch_availability();
    Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

void SScriptLevelSelectView::focus_selected_level() {
    if (level_buttons_.IsEmpty()) {
        focus_active_category();
        return;
    }
    auto const button_index{
        level_buttons_.IsValidIndex(selected_button_index_) ? selected_button_index_ : 0};
    focus_level(button_index);
}

auto SScriptLevelSelectView::OnFocusReceived(FGeometry const& geometry,
                                             FFocusEvent const& focus_event) -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    focus_selected_level();
    return FReply::Handled();
}

auto SScriptLevelSelectView::OnKeyDown(FGeometry const& geometry, FKeyEvent const& key_event)
    -> FReply {
    auto const key{key_event.GetKey()};
    auto const category_has_focus{mission_category_button_->has_focus() ||
                                  battle_viewer_category_button_->has_focus()};
    if (category_has_focus) {
        if (key == EKeys::Left || key == EKeys::Gamepad_DPad_Left) {
            if (category_ == ELevelCatalogCategory::BattleViewer) {
                handle_category_selected(ELevelCatalogCategory::Mission);
                focus_active_category();
                return FReply::Handled();
            }
        } else if (key == EKeys::Right || key == EKeys::Gamepad_DPad_Right) {
            if (category_ == ELevelCatalogCategory::Mission) {
                handle_category_selected(ELevelCatalogCategory::BattleViewer);
                focus_active_category();
                return FReply::Handled();
            }
        } else if (key == EKeys::Down || key == EKeys::Gamepad_DPad_Down) {
            focus_selected_level();
            return FReply::Handled();
        }
        return SCompoundWidget::OnKeyDown(geometry, key_event);
    }

    int32 current_index{INDEX_NONE};
    auto const button_count{level_buttons_.Num()};
    for (int32 index{}; index < button_count; ++index) {
        if (level_buttons_[index].IsValid() && level_buttons_[index]->has_focus()) {
            current_index = index;
            break;
        }
    }
    if (current_index == INDEX_NONE) {
        return SCompoundWidget::OnKeyDown(geometry, key_event);
    }
    if ((key == EKeys::Up || key == EKeys::Gamepad_DPad_Up) && current_index == 0) {
        focus_active_category();
        return FReply::Handled();
    }

    auto direction{0};
    if (key == EKeys::Up || key == EKeys::Gamepad_DPad_Up) {
        direction = -1;
    } else if (key == EKeys::Down || key == EKeys::Gamepad_DPad_Down) {
        direction = 1;
    }
    if (direction == 0 || level_buttons_.IsEmpty()) {
        return SCompoundWidget::OnKeyDown(geometry, key_event);
    }

    current_index = (current_index + direction + button_count) % button_count;
    focus_level(current_index);
    return FReply::Handled();
}

auto SScriptLevelSelectView::build_header() -> TSharedRef<SWidget> {
    auto const categories{
        SNew(SBorder)
            .BorderImage(&style_->chrome().frame_border)
            .Padding(FMargin{1.0f})
                [SNew(SHorizontalBox) +
                 SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(
                     150.0f)[SAssignNew(mission_category_button_, ml::ioj::SGameButton)
                                 .Style(&style_->button(EGameButtonStyle::Secondary))
                                 .Text(NSLOCTEXT("LevelSelect", "Missions", "MISSIONS"))
                                 .Selected(true)
                                 .OnClicked(this,
                                            &SScriptLevelSelectView::handle_category_selected,
                                            ELevelCatalogCategory::Mission)]] +
                 SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(
                     190.0f)[SAssignNew(battle_viewer_category_button_, ml::ioj::SGameButton)
                                 .Style(&style_->button(EGameButtonStyle::Secondary))
                                 .Text(NSLOCTEXT("LevelSelect", "BattleViewer", "BATTLE VIEWER"))
                                 .OnClicked(this,
                                            &SScriptLevelSelectView::handle_category_selected,
                                            ELevelCatalogCategory::BattleViewer)]]]};

    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().FillWidth(1.0f).VAlign(
               VAlign_Center)[SNew(STextBlock)
                                  .Text(NSLOCTEXT("LevelSelect",
                                                  "SystemContext",
                                                  "TACTICAL OPERATIONS // LEVEL CATALOG"))
                                  .TextStyle(&style_->text(EGameTextStyle::Caption))] +
           SHorizontalBox::Slot()
               .AutoWidth()
               .Padding(FMargin{0.0f, 0.0f, 12.0f, 0.0f})
               .VAlign(
                   VAlign_Center)[SNew(STextBlock)
                                      .Text(NSLOCTEXT("LevelSelect", "Title", "MISSION CONTROL //"))
                                      .TextStyle(&style_->text(EGameTextStyle::Heading3))] +
           SHorizontalBox::Slot().AutoWidth()[categories];
}

auto SScriptLevelSelectView::build_catalog() -> TSharedRef<SWidget> {
    auto const catalog{
        SNew(SVerticalBox) +
        SVerticalBox::Slot().AutoHeight().Padding(FMargin{
            0.0f, 0.0f, 0.0f, 4.0f})[SNew(STextBlock)
                                         .Text(this, &SScriptLevelSelectView::catalog_caption)
                                         .TextStyle(&style_->text(EGameTextStyle::Caption))] +
        SVerticalBox::Slot().AutoHeight().Padding(FMargin{
            0.0f, 0.0f, 0.0f, 16.0f})[SNew(STextBlock)
                                          .Text(this, &SScriptLevelSelectView::catalog_title)
                                          .TextStyle(&style_->text(EGameTextStyle::Heading3))] +
        SVerticalBox::Slot().FillHeight(
            1.0f)[SNew(SScrollBox).ScrollBarStyle(&style_->chrome().scroll_bar) +
                  SScrollBox::Slot()[SAssignNew(catalog_rows_, SVerticalBox)]]};

    return SNew(SBorder)
        .BorderImage(&style_->chrome().navigation_background)
        .Padding(
            FMargin{18.0f})[SNew(SBox).WidthOverride(350.0f).MinDesiredHeight(400.0f)[catalog]];
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
        180.0f)[SAssignNew(launch_button_, ml::ioj::SGameButton)
                    .Style(&style_->button(EGameButtonStyle::Primary))
                    .Text(this, &SScriptLevelSelectView::launch_text)
                    .OnClicked(this, &SScriptLevelSelectView::handle_action, on_launch_)]};
    auto const battle_speed{
        SAssignNew(battle_speed_control_, SVerticalBox) +
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
                                   80.0f)[SAssignNew(battle_speed_input_, SEditableText)
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
                       .Visibility(EVisibility::Collapsed)]};

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
                 FMargin{0.0f, 8.0f})[SNew(ml::ioj::SHiveSectionHeader)
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
    return category_ == ELevelCatalogCategory::Mission
             ? NSLOCTEXT("LevelSelect", "MissionCatalogCaption", "MISSION ARCHIVE")
             : NSLOCTEXT("LevelSelect", "BattleCatalogCaption", "BATTLE ARCHIVE");
}

auto SScriptLevelSelectView::catalog_title() const -> FText {
    return category_ == ELevelCatalogCategory::Mission
             ? NSLOCTEXT("LevelSelect", "MissionCatalogTitle", "OPERATIONS")
             : NSLOCTEXT("LevelSelect", "BattleCatalogTitle", "SCENARIOS");
}

auto SScriptLevelSelectView::directive_title() const -> FText {
    return category_ == ELevelCatalogCategory::Mission
             ? NSLOCTEXT("LevelSelect", "MissionDirective", "MISSION DIRECTIVE")
             : NSLOCTEXT("LevelSelect", "ScenarioDefinition", "SCENARIO DEFINITION");
}

auto SScriptLevelSelectView::launch_text() const -> FText {
    return category_ == ELevelCatalogCategory::Mission
             ? NSLOCTEXT("LevelSelect", "Launch", "Launch Mission")
             : NSLOCTEXT("LevelSelect", "ViewBattle", "View Battle");
}

auto SScriptLevelSelectView::handle_category_selected(ELevelCatalogCategory const category)
    -> FReply {
    category_ = category;
    update_category_selection();
    on_category_selected_.ExecuteIfBound(category);
    return FReply::Handled();
}

auto SScriptLevelSelectView::handle_level_selected(int32 const button_index) -> FReply {
    selected_button_index_ = button_index;
    update_selection(button_index);
    on_level_selected_.ExecuteIfBound(button_index);
    return FReply::Handled();
}

auto SScriptLevelSelectView::handle_action(FSimpleDelegate delegate) -> FReply {
    delegate.ExecuteIfBound();
    return FReply::Handled();
}

void SScriptLevelSelectView::handle_battle_speed_changed(FText const& text) {
    auto value_text{text.ToString()};
    value_text.TrimStartAndEndInline();
    auto const numeric{value_text.IsNumeric()};
    auto const value{numeric ? FCString::Atod(*value_text) : 0.0};
    battle_speed_valid_ = numeric && ml::ioj::level_launch::is_valid_time_scale(value);
    battle_speed_error_->SetVisibility(battle_speed_valid_ ? EVisibility::Collapsed
                                                           : EVisibility::HitTestInvisible);
    update_launch_availability();
    on_battle_speed_changed_.ExecuteIfBound(battle_speed_valid_ ? TOptional<double>{value}
                                                                : TOptional<double>{});
}

void SScriptLevelSelectView::update_launch_availability() {
    auto const speed_is_valid{category_ != ELevelCatalogCategory::BattleViewer ||
                              battle_speed_valid_};
    launch_button_->SetEnabled(selected_level_can_launch_ && speed_is_valid);
}

void SScriptLevelSelectView::rebuild_catalog(FLevelSelectViewState const& state) {
    catalog_rows_->ClearChildren();
    level_buttons_.Reset();

    for (auto const& row : state.rows) {
        if (row.heading) {
            catalog_rows_->AddSlot().AutoHeight().Padding(FMargin{4.0f, 14.0f, 4.0f, 6.0f})
                [SNew(STextBlock).Text(row.text).TextStyle(&style_->text(EGameTextStyle::Caption))];
            continue;
        }

        auto const button_index{level_buttons_.Num()};
        TSharedPtr<ml::ioj::SGameButton> button;
        catalog_rows_->AddSlot().AutoHeight().Padding(
            FMargin{0.0f, 0.0f, 0.0f, style_->chrome().navigation_spacing})
            [SAssignNew(button, ml::ioj::SGameButton)
                 .Style(&style_->button(EGameButtonStyle::Secondary))
                 .ContentAlignment(HAlign_Left)
                 .Text(row.text)
                 .Selected(button_index == state.selected_button_index)
                 .OnClicked(this, &SScriptLevelSelectView::handle_level_selected, button_index)];
        level_buttons_.Add(button);
    }
}

void SScriptLevelSelectView::update_selection(int32 const button_index) {
    auto const button_count{level_buttons_.Num()};
    for (int32 index{}; index < button_count; ++index) {
        level_buttons_[index]->set_selected(index == button_index);
    }
}

void SScriptLevelSelectView::update_category_selection() {
    if (mission_category_button_.IsValid()) {
        mission_category_button_->set_selected(category_ == ELevelCatalogCategory::Mission);
    }
    if (battle_viewer_category_button_.IsValid()) {
        battle_viewer_category_button_->set_selected(category_ ==
                                                     ELevelCatalogCategory::BattleViewer);
    }
}

void SScriptLevelSelectView::focus_active_category() {
    auto const& button{category_ == ELevelCatalogCategory::Mission
                           ? mission_category_button_
                           : battle_viewer_category_button_};
    if (button.IsValid()) {
        button->focus();
    }
}

void SScriptLevelSelectView::focus_level(int32 const button_index) {
    if (level_buttons_.IsValidIndex(button_index) && level_buttons_[button_index].IsValid()) {
        level_buttons_[button_index]->focus();
    }
}
} // namespace ml::s7
