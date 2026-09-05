#include "SpaceGame/ui/common/HiveWidgets.h"

#include "SpaceGame/ui/common/SGameButton.h"

#include <Widgets/Images/SImage.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Text/STextBlock.h>

namespace ml::ioj {
void SHiveFrame::Construct(FArguments const& args) {
    check(args._Style != nullptr);
    auto const& style{*args._Style};
    ChildSlot[SNew(SBox)
                  .WidthOverride(args._WidthOverride)
                  .HeightOverride(args._HeightOverride)
                      [SNew(SBorder)
                           .BorderImage(&style.frame_border)
                           .Padding(style.frame_border_thickness)
                               [SNew(SBorder)
                                    .BorderImage(&style.frame_background)
                                    .Padding(style.frame_padding)[args._Content.Widget]]]];
}

void SHiveSectionHeader::Construct(FArguments const& args) {
    check(args._Style != nullptr);
    auto const& style{*args._Style};
    ChildSlot[SNew(SHorizontalBox) +
              SHorizontalBox::Slot().AutoWidth().VAlign(
                  VAlign_Center)[SNew(SImage)
                                     .Image(args._Icon)
                                     .ColorAndOpacity(style.palette().honey)
                                     .DesiredSizeOverride(FVector2D{20.0f, 20.0f})
                                     .Visibility(args._Icon != nullptr
                                                     ? EVisibility::HitTestInvisible
                                                     : EVisibility::Collapsed)] +
              SHorizontalBox::Slot()
                  .FillWidth(1.0f)
                  .Padding(FMargin{args._Icon != nullptr ? 10.0f : 0.0f, 0.0f})
                  .VAlign(VAlign_Center)[SNew(STextBlock)
                                             .Text(args._Text)
                                             .TextStyle(&style.text(EGameTextStyle::Heading3))]];
}

void SHiveNavigationButton::Construct(FArguments const& args) {
    check(args._Style != nullptr);
    style_ = args._Style;
    selected_ = args._Selected;
    auto const& style{*args._Style};
    ChildSlot[SAssignNew(button_, SGameButton)
                  .Style(&style.button(EGameButtonStyle::Secondary))
                  .Icon(args._Icon)
                  .IconTint(this, &SHiveNavigationButton::icon_colour)
                  .ContentAlignment(HAlign_Left)
                  .Text(args._Text)
                  .Enabled(args._Enabled)
                  .Selected(args._Selected)
                  .OnClicked(args._OnClicked)];
}

void SHiveNavigationButton::set_selected(bool const selected) {
    selected_ = selected;
    if (button_.IsValid()) {
        button_->set_selected(selected);
    }
}

void SHiveNavigationButton::focus() {
    if (button_.IsValid()) {
        button_->focus();
    }
}

auto SHiveNavigationButton::has_focus() const -> bool {
    return button_.IsValid() && button_->has_focus();
}

auto SHiveNavigationButton::icon_colour() const -> FSlateColor {
    if (selected_) {
        return FSlateColor{style_->palette().control};
    }
    if (has_focus()) {
        return FSlateColor{style_->palette().focus};
    }
    return FSlateColor{style_->palette().text_primary};
}
}
