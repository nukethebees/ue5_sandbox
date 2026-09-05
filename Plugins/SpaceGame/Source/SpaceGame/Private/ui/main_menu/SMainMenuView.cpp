#include "SMainMenuView.h"

#include "SpaceGame/ui/common/HiveWidgets.h"
#include "SpaceGame/ui/common/SGameButton.h"

#include "InputCoreTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ml::ioj {
void SMainMenuView::Construct(FArguments const& args) {
    style_ = args._Style;
    focused_action_ = args._InitialAction;
    check(style_ != nullptr);
    on_select_mission_ = args._OnSelectMission;
    on_save_data_ = args._OnSaveData;
    on_options_ = args._OnOptions;
    on_quit_game_ = args._OnQuitGame;

    auto const header{build_header()};
    auto const navigation{build_navigation()};
    auto const identity{build_identity()};
    auto const footer{build_footer()};

    auto const body{
        SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[navigation] +
        SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin{2.0f, 0.0f, 0.0f, 0.0f})[identity]};

    auto const frame{
        SNew(SHiveFrame)
            .Style(&style_->chrome())
            .WidthOverride(1120.0f)
            .HeightOverride(640.0f)[SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[header] +
                                    SVerticalBox::Slot().FillHeight(1.0f)[body] +
                                    SVerticalBox::Slot().AutoHeight()[footer]]};

    ChildSlot[SNew(SBorder)
                  .BorderImage(&style_->chrome().canvas)
                  .Padding(
                      FMargin{16.0f})[SNew(SScaleBox)
                                          .Stretch(EStretch::ScaleToFit)
                                          .StretchDirection(EStretchDirection::DownOnly)[frame]]];
}

void SMainMenuView::focus_action(EMainMenuAction const action) {
    focused_action_ = action;
    auto const index{static_cast<int32>(action)};
    if (action_buttons_.IsValidIndex(index) && action_buttons_[index].IsValid()) {
        action_buttons_[index]->focus();
    }
}

auto SMainMenuView::OnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
    -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    focus_action(focused_action_);
    return FReply::Handled();
}

auto SMainMenuView::OnKeyDown(FGeometry const& geometry, FKeyEvent const& key_event) -> FReply {
    auto direction{0};
    auto const key{key_event.GetKey()};
    if (key == EKeys::Up || key == EKeys::Gamepad_DPad_Up) {
        direction = -1;
    } else if (key == EKeys::Down || key == EKeys::Gamepad_DPad_Down) {
        direction = 1;
    }
    if (direction == 0) {
        return SCompoundWidget::OnKeyDown(geometry, key_event);
    }

    auto const action_count{action_buttons_.Num()};
    auto current_index{static_cast<int32>(focused_action_)};
    for (int32 index{}; index < action_count; ++index) {
        if (action_buttons_[index].IsValid() && action_buttons_[index]->has_focus()) {
            current_index = index;
            break;
        }
    }
    auto const next_index{(current_index + direction + action_count) % action_count};
    focus_action(static_cast<EMainMenuAction>(next_index));
    return FReply::Handled();
}

auto SMainMenuView::build_header() const -> TSharedRef<SWidget> {
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
                 .VAlign(VAlign_Center)[SNew(STextBlock)
                                            .Text(NSLOCTEXT("MainMenu", "Title", "HIVE SYSTEMS"))
                                            .TextStyle(&style_->text(EGameTextStyle::Heading2))] +
             SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                 [SNew(STextBlock)
                      .Text(
                          NSLOCTEXT("MainMenu", "SystemContext", "TACTICAL OPERATIONS // COMMAND"))
                      .TextStyle(&style_->text(EGameTextStyle::Caption))]];
}

auto SMainMenuView::build_navigation() -> TSharedRef<SWidget> {
    auto actions{SNew(SVerticalBox)};
    actions->AddSlot().AutoHeight().Padding(
        FMargin{0.0f,
                0.0f,
                0.0f,
                4.0f})[SNew(STextBlock)
                           .Text(NSLOCTEXT("MainMenu", "NavigationCaption", "COMMAND DECK"))
                           .TextStyle(&style_->text(EGameTextStyle::Caption))];
    actions->AddSlot().AutoHeight().Padding(FMargin{
        0.0f, 0.0f, 0.0f, 20.0f})[SNew(STextBlock)
                                      .Text(NSLOCTEXT("MainMenu", "NavigationTitle", "OPERATIONS"))
                                      .TextStyle(&style_->text(EGameTextStyle::Heading3))];

    add_action(actions,
               EMainMenuAction::SelectMission,
               NSLOCTEXT("MainMenu", "SelectMission", "Select Mission"),
               on_select_mission_);
    add_action(actions,
               EMainMenuAction::SaveData,
               NSLOCTEXT("MainMenu", "SaveData", "Save Data"),
               on_save_data_);
    add_action(actions,
               EMainMenuAction::Options,
               NSLOCTEXT("MainMenu", "Options", "Options"),
               on_options_);
    actions->AddSlot().FillHeight(1.0f);
    add_action(actions,
               EMainMenuAction::QuitGame,
               NSLOCTEXT("MainMenu", "QuitGame", "Quit Game"),
               on_quit_game_);
    actions->AddSlot().AutoHeight().Padding(FMargin{
        0.0f,
        16.0f,
        0.0f,
        0.0f})[SNew(STextBlock)
                   .Text(NSLOCTEXT("MainMenu", "NavigationHint", "UP / DOWN  //  ENTER TO SELECT"))
                   .TextStyle(&style_->text(EGameTextStyle::Caption))];

    return SNew(SBorder)
        .BorderImage(&style_->chrome().navigation_background)
        .Padding(
            FMargin{18.0f})[SNew(SBox).WidthOverride(280.0f).MinDesiredHeight(400.0f)[actions]];
}

auto SMainMenuView::build_identity() const -> TSharedRef<SWidget> {
    return SNew(SBorder)
        .BorderImage(&style_->chrome().body_background)
        .Padding(FMargin{48.0f})
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
            [SNew(SVerticalBox) +
             SVerticalBox::Slot().AutoHeight().HAlign(
                 HAlign_Center)[SNew(SImage)
                                    .Image(&style_->icon(EGameUiIcon::Hive))
                                    .ColorAndOpacity(style_->palette().honey)
                                    .DesiredSizeOverride(FVector2D{172.0f, 172.0f})] +
             SVerticalBox::Slot()
                 .AutoHeight()
                 .HAlign(HAlign_Center)
                 .Padding(FMargin{0.0f, 22.0f, 0.0f, 0.0f})
                     [SNew(STextBlock)
                          .Text(NSLOCTEXT("MainMenu", "IdentityTitle", "HIVE SYSTEMS"))
                          .TextStyle(&style_->text(EGameTextStyle::Heading1))] +
             SVerticalBox::Slot()
                 .AutoHeight()
                 .HAlign(HAlign_Center)
                 .Padding(FMargin{0.0f, 8.0f, 0.0f, 18.0f})
                     [SNew(STextBlock)
                          .Text(NSLOCTEXT(
                              "MainMenu", "IdentitySubtitle", "TACTICAL OPERATIONS INTERFACE"))
                          .TextStyle(&style_->text(EGameTextStyle::Heading3))] +
             SVerticalBox::Slot().AutoHeight().HAlign(
                 HAlign_Center)[SNew(SBox).WidthOverride(260.0f).HeightOverride(
                 2.0f)[SNew(SBorder).BorderImage(&style_->chrome().focus)]] +
             SVerticalBox::Slot()
                 .AutoHeight()
                 .HAlign(HAlign_Center)
                 .Padding(FMargin{0.0f, 18.0f, 0.0f, 0.0f})
                     [SNew(STextBlock)
                          .Text(
                              NSLOCTEXT("MainMenu",
                                        "IdentityDetail",
                                        "MISSION CONTROL // DATA ARCHIVE // SYSTEM CONFIGURATION"))
                          .TextStyle(&style_->text(EGameTextStyle::Caption))
                          .Justification(ETextJustify::Center)
                          .AutoWrapText(true)
                          .WrapTextAt(540.0f)]];
}

auto SMainMenuView::build_footer() const -> TSharedRef<SWidget> {
    return SNew(SBorder)
        .BorderImage(&style_->chrome().footer_background)
        .Padding(style_->chrome().footer_padding)
            [SNew(SHorizontalBox) +
             SHorizontalBox::Slot().FillWidth(1.0f)
                 [SNew(STextBlock)
                      .Text(NSLOCTEXT("MainMenu", "FooterContext", "HIVE // COMMAND INTERFACE"))
                      .TextStyle(&style_->text(EGameTextStyle::Caption))] +
             SHorizontalBox::Slot().AutoWidth()
                 [SNew(STextBlock)
                      .Text(NSLOCTEXT("MainMenu", "FooterStatus", "SYSTEM STATUS // ONLINE"))
                      .TextStyle(&style_->text(EGameTextStyle::Caption))]];
}

void SMainMenuView::add_action(TSharedRef<SVerticalBox> const& actions,
                               EMainMenuAction const action,
                               FText text,
                               FSimpleDelegate delegate) {
    auto button{SNew(SGameButton)
                    .Style(&style_->button(EGameButtonStyle::Secondary))
                    .ContentAlignment(HAlign_Left)
                    .Text(MoveTemp(text))
                    .OnClicked(this, &SMainMenuView::activate_action, action, MoveTemp(delegate))};
    actions->AddSlot().AutoHeight().Padding(
        FMargin{0.0f, 0.0f, 0.0f, style_->chrome().navigation_spacing})[button];
    action_buttons_.Add(button);
}

auto SMainMenuView::activate_action(EMainMenuAction const action, FSimpleDelegate delegate)
    -> FReply {
    focused_action_ = action;
    delegate.ExecuteIfBound();
    return FReply::Handled();
}
} // namespace ml::ioj
