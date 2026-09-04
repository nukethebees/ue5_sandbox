#pragma once

#include "Layout/Margin.h"
#include "Widgets/SBoxPanel.h"

namespace SandboxUI::Slate {
inline auto vbox_auto_slot(FMargin padding = {}) -> SVerticalBox::FSlot::FSlotArguments {
    return MoveTemp(SVerticalBox::Slot().AutoHeight().Padding(padding));
}

inline auto vbox_fill_slot(float const fill_height = 1.0f, FMargin padding = {})
    -> SVerticalBox::FSlot::FSlotArguments {
    return MoveTemp(SVerticalBox::Slot().FillHeight(fill_height).Padding(padding));
}

inline auto hbox_auto_slot(FMargin padding = {}) -> SHorizontalBox::FSlot::FSlotArguments {
    return MoveTemp(SHorizontalBox::Slot().AutoWidth().Padding(padding));
}

inline auto hbox_fill_slot(float const fill_width = 1.0f, FMargin padding = {})
    -> SHorizontalBox::FSlot::FSlotArguments {
    return MoveTemp(SHorizontalBox::Slot().FillWidth(fill_width).Padding(padding));
}
}
