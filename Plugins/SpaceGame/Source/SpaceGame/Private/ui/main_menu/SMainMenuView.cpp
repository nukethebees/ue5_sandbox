#include "SMainMenuView.h"

#include "SpaceGame/ui/common/HiveWidgets.h"

#include "InputCoreTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ml::ioj {
void SMainMenuView::Construct(FArguments const& args) {
    style_ = args._Style;
    active_page_ = args._InitialPage;
    check(style_ != nullptr);
    on_page_selected_ = args._OnPageSelected;
    on_quit_ = args._OnQuit;
    on_focus_content_ = args._OnFocusContent;

    SAssignNew(mission_content_, SBox)[args._MissionContent.Widget];
    SAssignNew(content_switcher_, SWidgetSwitcher) +
        SWidgetSwitcher::Slot()[mission_content_.ToSharedRef()] +
        SWidgetSwitcher::Slot()[args._ArchiveContent.Widget] +
        SWidgetSwitcher::Slot()[args._TelemetryContent.Widget] +
        SWidgetSwitcher::Slot()[args._OptionsContent.Widget] +
        SWidgetSwitcher::Slot()[args._DebugContent.Widget];

    auto const body{SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[build_navigation()] +
                    SHorizontalBox::Slot().FillWidth(1.0f).Padding(
                        FMargin{2.0f, 0.0f})[content_switcher_.ToSharedRef()]};

    ChildSlot[SNew(SBorder)
                  .BorderImage(&style_->chrome().canvas)
                  .Padding(FMargin{})[SNew(SHiveFrame)
                                          .Style(&style_->chrome())
                                              [SNew(SVerticalBox) +
                                               SVerticalBox::Slot().AutoHeight()[build_header()] +
                                               SVerticalBox::Slot().FillHeight(1.0f)[body] +
                                               SVerticalBox::Slot().AutoHeight()[build_footer()]]]];
    set_active_page(active_page_);
}

void SMainMenuView::set_active_page(EMainMenuPage const page) {
    active_page_ = page;
    if (content_switcher_.IsValid()) {
        content_switcher_->SetActiveWidgetIndex(page_content_index(page));
    }

    auto const active_index{static_cast<int32>(page)};
    auto const button_count{page_buttons_.Num()};
    for (int32 index{}; index < button_count; ++index) {
        if (page_buttons_[index].IsValid()) {
            page_buttons_[index]->set_selected(index == active_index);
        }
    }
}

void SMainMenuView::set_mission_content(TSharedRef<SWidget> content) {
    if (mission_content_.IsValid()) {
        mission_content_->SetContent(MoveTemp(content));
    }
}

void SMainMenuView::set_navigation_enabled(bool const enabled) {
    navigation_enabled_ = enabled;
    for (auto const& button : page_buttons_) {
        if (button.IsValid()) {
            button->SetEnabled(enabled);
        }
    }
    if (quit_button_.IsValid()) {
        quit_button_->SetEnabled(enabled);
    }
}

void SMainMenuView::focus_navigation() {
    auto const index{static_cast<int32>(active_page_)};
    if (page_buttons_.IsValidIndex(index) && page_buttons_[index].IsValid()) {
        page_buttons_[index]->focus();
    }
}

void SMainMenuView::focus_content_on_next_focus() {
    focus_content_next_ = true;
}

auto SMainMenuView::OnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
    -> FReply {
    static_cast<void>(geometry);
    static_cast<void>(focus_event);
    if (focus_content_next_) {
        focus_content_next_ = false;
        on_focus_content_.ExecuteIfBound();
    } else {
        focus_navigation();
    }
    return FReply::Handled();
}

auto SMainMenuView::OnKeyDown(FGeometry const& geometry, FKeyEvent const& key_event) -> FReply {
    auto const key{key_event.GetKey()};
    if (navigation_enabled_ && navigation_has_focus() &&
        (key == EKeys::Right || key == EKeys::Gamepad_DPad_Right)) {
        on_focus_content_.ExecuteIfBound();
        return FReply::Handled();
    }
    if (navigation_enabled_ && !navigation_has_focus() &&
        (key == EKeys::Left || key == EKeys::Gamepad_DPad_Left)) {
        focus_navigation();
        return FReply::Handled();
    }
    if (!navigation_enabled_ || !navigation_has_focus()) {
        return SCompoundWidget::OnKeyDown(geometry, key_event);
    }

    auto direction{0};
    if (key == EKeys::Up || key == EKeys::Gamepad_DPad_Up) {
        direction = -1;
    } else if (key == EKeys::Down || key == EKeys::Gamepad_DPad_Down) {
        direction = 1;
    }
    if (direction == 0) {
        return SCompoundWidget::OnKeyDown(geometry, key_event);
    }

    auto buttons{page_buttons_};
    buttons.Add(quit_button_);
    auto current_index{static_cast<int32>(active_page_)};
    auto const button_count{buttons.Num()};
    for (int32 index{}; index < button_count; ++index) {
        if (buttons[index].IsValid() && buttons[index]->has_focus()) {
            current_index = index;
            break;
        }
    }
    auto const next_index{(current_index + direction + button_count) % button_count};
    if (buttons[next_index].IsValid()) {
        buttons[next_index]->focus();
    }
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
    auto navigation{SNew(SVerticalBox)};
    navigation->AddSlot().AutoHeight().Padding(
        FMargin{0.0f,
                0.0f,
                0.0f,
                4.0f})[SNew(STextBlock)
                           .Text(NSLOCTEXT("MainMenu", "NavigationCaption", "COMMAND DECK"))
                           .TextStyle(&style_->text(EGameTextStyle::Caption))];
    add_section(navigation, NSLOCTEXT("MainMenu", "OperationsSection", "OPERATIONS"));
    add_page(navigation,
             EMainMenuPage::SelectMission,
             NSLOCTEXT("MainMenu", "SelectMission", "Select Mission"),
             EGameUiIcon::Hive);
    add_page(navigation,
             EMainMenuPage::DataArchive,
             NSLOCTEXT("MainMenu", "DataArchive", "Data Archive"),
             EGameUiIcon::Hive);
    add_page(navigation,
             EMainMenuPage::Telemetry,
             NSLOCTEXT("MainMenu", "Telemetry", "Telemetry"),
             EGameUiIcon::Hive);

    add_section(navigation, NSLOCTEXT("MainMenu", "ConfigurationSection", "CONFIGURATION"));
    add_page(navigation,
             EMainMenuPage::Video,
             NSLOCTEXT("MainMenu", "Video", "Video"),
             EGameUiIcon::Video);
    add_page(navigation,
             EMainMenuPage::Gameplay,
             NSLOCTEXT("MainMenu", "Gameplay", "Gameplay"),
             EGameUiIcon::Gameplay);
    add_page(navigation,
             EMainMenuPage::Audio,
             NSLOCTEXT("MainMenu", "Audio", "Audio"),
             EGameUiIcon::Audio);
    add_page(navigation,
             EMainMenuPage::Controls,
             NSLOCTEXT("MainMenu", "Controls", "Controls"),
             EGameUiIcon::Controls);
    add_page(navigation,
             EMainMenuPage::Accessibility,
             NSLOCTEXT("MainMenu", "Accessibility", "Accessibility"),
             EGameUiIcon::Accessibility);
    add_page(navigation,
             EMainMenuPage::System,
             NSLOCTEXT("MainMenu", "System", "System"),
             EGameUiIcon::System);
    add_page(navigation,
             EMainMenuPage::Debug,
             NSLOCTEXT("MainMenu", "Debug", "Debug"),
             EGameUiIcon::Gameplay);

    navigation->AddSlot().FillHeight(1.0f);
    navigation->AddSlot().AutoHeight().Padding(
        FMargin{0.0f,
                0.0f,
                0.0f,
                style_->chrome()
                    .navigation_spacing})[SAssignNew(quit_button_, SHiveNavigationButton)
                                              .Style(style_)
                                              .Icon(&style_->icon(EGameUiIcon::System))
                                              .Text(NSLOCTEXT("MainMenu", "QuitGame", "Quit Game"))
                                              .OnClicked(this, &SMainMenuView::handle_quit)];
    navigation->AddSlot().AutoHeight().Padding(FMargin{
        0.0f,
        14.0f,
        0.0f,
        0.0f})[SNew(STextBlock)
                   .Text(NSLOCTEXT("MainMenu", "NavigationHint", "UP / DOWN  //  RIGHT TO ENTER"))
                   .TextStyle(&style_->text(EGameTextStyle::Caption))];

    return SNew(SBorder)
        .BorderImage(&style_->chrome().navigation_background)
        .Padding(FMargin{
            14.0f})[SNew(SBox).WidthOverride(style_->chrome().navigation_width)[navigation]];
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

void SMainMenuView::add_section(TSharedRef<SVerticalBox> const& navigation, FText text) const {
    navigation->AddSlot().AutoHeight().Padding(FMargin{0.0f, 12.0f, 0.0f, 8.0f})
        [SNew(STextBlock).Text(MoveTemp(text)).TextStyle(&style_->text(EGameTextStyle::Caption))];
}

void SMainMenuView::add_page(TSharedRef<SVerticalBox> const& navigation,
                             EMainMenuPage const page,
                             FText text,
                             EGameUiIcon const icon) {
    TSharedPtr<SHiveNavigationButton> button;
    navigation->AddSlot().AutoHeight().Padding(
        FMargin{0.0f,
                0.0f,
                0.0f,
                style_->chrome()
                    .navigation_spacing})[SAssignNew(button, SHiveNavigationButton)
                                              .Style(style_)
                                              .Icon(&style_->icon(icon))
                                              .Text(MoveTemp(text))
                                              .OnClicked(this, &SMainMenuView::handle_page, page)];
    page_buttons_.Add(button);
}

auto SMainMenuView::handle_page(EMainMenuPage const page) -> FReply {
    on_page_selected_.ExecuteIfBound(page);
    return FReply::Handled();
}

auto SMainMenuView::handle_quit() -> FReply {
    on_quit_.ExecuteIfBound();
    return FReply::Handled();
}

auto SMainMenuView::page_content_index(EMainMenuPage const page) const -> int32 {
    if (page == EMainMenuPage::SelectMission) {
        return 0;
    }
    if (page == EMainMenuPage::DataArchive) {
        return 1;
    }
    if (page == EMainMenuPage::Telemetry) {
        return 2;
    }
    if (page == EMainMenuPage::Debug) {
        return 4;
    }
    return 3;
}

auto SMainMenuView::navigation_has_focus() const -> bool {
    for (auto const& button : page_buttons_) {
        if (button.IsValid() && button->has_focus()) {
            return true;
        }
    }
    return quit_button_.IsValid() && quit_button_->has_focus();
}
} // namespace ml::ioj
