#include "SScriptLevelSelectView.h"

#include <SpaceGame/ui/common/HiveWidgets.h>
#include <SpaceGame/ui/common/SGameButton.h>

#include <InputCoreTypes.h>
#include <Widgets/Images/SImage.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Text/STextBlock.h>

namespace ml::s7 {
namespace {
auto action_button(TSharedPtr<ml::ioj::SGameButton>& button,
                   ml::ioj::FGameUiStyle const& style,
                   EGameButtonStyle const button_style,
                   FText text,
                   FOnClicked on_clicked) -> TSharedRef<SWidget> {
    return SNew(SBox).WidthOverride(180.0f)[SAssignNew(button, ml::ioj::SGameButton)
                                                .Style(&style.button(button_style))
                                                .Text(MoveTemp(text))
                                                .OnClicked(MoveTemp(on_clicked))];
}
}

void SScriptLevelSelectView::Construct(FArguments const& args) {
    style_ = args._Style;
    check(style_ != nullptr);
    on_level_selected_ = args._OnLevelSelected;
    on_refresh_ = args._OnRefresh;
    on_launch_ = args._OnLaunch;
    on_start_paused_ = args._OnStartPaused;
    on_back_ = args._OnBack;

    auto const body{SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[build_catalog()] +
                    SHorizontalBox::Slot().FillWidth(1.0f).Padding(
                        FMargin{2.0f, 0.0f, 0.0f, 0.0f})[build_details()]};

    ChildSlot[SNew(SBorder)
                  .BorderImage(&style_->chrome().canvas)
                  .Padding(FMargin{})[SNew(ml::ioj::SHiveFrame)
                                          .Style(&style_->chrome())
                                              [SNew(SVerticalBox) +
                                               SVerticalBox::Slot().AutoHeight()[build_header()] +
                                               SVerticalBox::Slot().FillHeight(1.0f)[body] +
                                               SVerticalBox::Slot().AutoHeight()[build_footer()]]]];
}

void SScriptLevelSelectView::replace_catalog(FLevelSelectViewState const& state) {
    rebuild_catalog(state);
    update_state(state);
}

void SScriptLevelSelectView::update_state(FLevelSelectViewState const& state) {
    selected_button_index_ = state.selected_button_index;
    update_selection(selected_button_index_);

    title_->SetText(state.title);
    status_->SetText(state.status);
    description_->SetText(state.description);
    filename_->SetText(state.filename);
    details_->SetText(state.details);
    script_->SetText(state.script);
    launch_button_->SetEnabled(state.can_launch);
    start_paused_button_->SetEnabled(state.can_launch);
}

void SScriptLevelSelectView::focus_selected_level() {
    if (level_buttons_.IsEmpty()) {
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
    auto direction{0};
    auto const key{key_event.GetKey()};
    if (key == EKeys::Up || key == EKeys::Gamepad_DPad_Up) {
        direction = -1;
    } else if (key == EKeys::Down || key == EKeys::Gamepad_DPad_Down) {
        direction = 1;
    }
    if (direction == 0 || level_buttons_.IsEmpty()) {
        return SCompoundWidget::OnKeyDown(geometry, key_event);
    }

    auto current_index{selected_button_index_};
    auto const button_count{level_buttons_.Num()};
    for (int32 index{}; index < button_count; ++index) {
        if (level_buttons_[index].IsValid() && level_buttons_[index]->has_focus()) {
            current_index = index;
            break;
        }
    }
    if (!level_buttons_.IsValidIndex(current_index)) {
        current_index = direction > 0 ? 0 : button_count - 1;
    } else {
        current_index = (current_index + direction + button_count) % button_count;
    }
    focus_level(current_index);
    return FReply::Handled();
}

auto SScriptLevelSelectView::build_header() const -> TSharedRef<SWidget> {
    return SNew(SBorder)
        .BorderImage(&style_->chrome().header_background)
        .Padding(style_->chrome().header_padding)
            [SNew(SHorizontalBox) +
             SHorizontalBox::Slot().AutoWidth().VAlign(
                 VAlign_Center)[SNew(SImage)
                                    .Image(&style_->icon(EGameUiIcon::Hive))
                                    .ColorAndOpacity(style_->palette().honey)
                                    .DesiredSizeOverride(FVector2D{28.0f, 28.0f})] +
             SHorizontalBox::Slot()
                 .FillWidth(1.0f)
                 .Padding(FMargin{14.0f, 0.0f})
                 .VAlign(
                     VAlign_Center)[SNew(STextBlock)
                                        .Text(NSLOCTEXT("LevelSelect", "Title", "MISSION CONTROL"))
                                        .TextStyle(&style_->text(EGameTextStyle::Heading2))] +
             SHorizontalBox::Slot().AutoWidth().VAlign(
                 VAlign_Center)[SNew(STextBlock)
                                    .Text(NSLOCTEXT("LevelSelect",
                                                    "SystemContext",
                                                    "TACTICAL OPERATIONS // LEVEL CATALOG"))
                                    .TextStyle(&style_->text(EGameTextStyle::Caption))]];
}

auto SScriptLevelSelectView::build_catalog() -> TSharedRef<SWidget> {
    auto const catalog{
        SNew(SVerticalBox) +
        SVerticalBox::Slot().AutoHeight().Padding(
            FMargin{0.0f,
                    0.0f,
                    0.0f,
                    4.0f})[SNew(STextBlock)
                               .Text(NSLOCTEXT("LevelSelect", "CatalogCaption", "MISSION ARCHIVE"))
                               .TextStyle(&style_->text(EGameTextStyle::Caption))] +
        SVerticalBox::Slot().AutoHeight().Padding(
            FMargin{0.0f,
                    0.0f,
                    0.0f,
                    16.0f})[SNew(STextBlock)
                                .Text(NSLOCTEXT("LevelSelect", "CatalogTitle", "OPERATIONS"))
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
             SVerticalBox::Slot().AutoHeight().Padding(FMargin{
                 0.0f,
                 8.0f})[SNew(ml::ioj::SHiveSectionHeader)
                            .Style(style_)
                            .Text(NSLOCTEXT("LevelSelect", "Directive", "MISSION DIRECTIVE"))] +
             SVerticalBox::Slot().FillHeight(1.0f)[script_panel]];
}

auto SScriptLevelSelectView::build_footer() -> TSharedRef<SWidget> {
    TSharedPtr<ml::ioj::SGameButton> back_button;
    TSharedPtr<ml::ioj::SGameButton> refresh_button;
    auto const back{action_button(
        back_button,
        *style_,
        EGameButtonStyle::Secondary,
        NSLOCTEXT("LevelSelect", "Back", "Back"),
        FOnClicked::CreateSP(this, &SScriptLevelSelectView::handle_action, on_back_))};
    auto const refresh{action_button(
        refresh_button,
        *style_,
        EGameButtonStyle::Secondary,
        NSLOCTEXT("LevelSelect", "Refresh", "Refresh Catalog"),
        FOnClicked::CreateSP(this, &SScriptLevelSelectView::handle_action, on_refresh_))};
    auto const paused{action_button(
        start_paused_button_,
        *style_,
        EGameButtonStyle::Secondary,
        NSLOCTEXT("LevelSelect", "StartPaused", "Stage Paused"),
        FOnClicked::CreateSP(this, &SScriptLevelSelectView::handle_action, on_start_paused_))};
    auto const launch{action_button(
        launch_button_,
        *style_,
        EGameButtonStyle::Primary,
        NSLOCTEXT("LevelSelect", "Launch", "Launch Mission"),
        FOnClicked::CreateSP(this, &SScriptLevelSelectView::handle_action, on_launch_))};

    return SNew(SBorder)
        .BorderImage(&style_->chrome().footer_background)
        .Padding(style_->chrome().footer_padding)
            [SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[back] +
             SHorizontalBox::Slot().FillWidth(1.0f) +
             SHorizontalBox::Slot().AutoWidth().Padding(FMargin{0.0f, 0.0f, 10.0f, 0.0f})[refresh] +
             SHorizontalBox::Slot().AutoWidth().Padding(FMargin{0.0f, 0.0f, 10.0f, 0.0f})[paused] +
             SHorizontalBox::Slot().AutoWidth()[launch]];
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

void SScriptLevelSelectView::focus_level(int32 const button_index) {
    if (level_buttons_.IsValidIndex(button_index) && level_buttons_[button_index].IsValid()) {
        level_buttons_[button_index]->focus();
    }
}
} // namespace ml::s7
