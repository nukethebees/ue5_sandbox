#include "Sandbox/slate_widgets/SInGameMenuWidget.h"

#include "SlateOptMacros.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Sandbox/slate_widgets/SInventoryTabWidget.h"
#include "Sandbox/slate_widgets/SStatsTabWidget.h"
#include "Sandbox/utilities/SandboxStyle.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SInGameMenuWidget::Construct(FArguments const& InArgs) {
    // clang-format off
    ChildSlot[
        SNew(SBox)
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill)
            .Padding(FMargin{50.0f})
            [
                SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
                    .Padding(FMargin{20.0f})
                    [
                        SNew(SVerticalBox)
                        // Tab buttons
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(FMargin{0.0f, 0.0f, 0.0f, 10.0f})
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .Padding(FMargin{0.0f, 0.0f, 5.0f, 0.0f})
                                    [
                                        SNew(SButton)
                                            .OnClicked(this, &SInGameMenuWidget::on_stats_tab_clicked)
                                            [
                                                SNew(STextBlock)
                                                    .Text(FText::FromString(TEXT("Stats")))
                                            ]
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .Padding(FMargin{5.0f, 0.0f, 0.0f, 0.0f})
                                    [
                                        SNew(SButton)
                                            .OnClicked(this, &SInGameMenuWidget::on_inventory_tab_clicked)
                                            [
                                                SNew(STextBlock)
                                                    .Text(FText::FromString(TEXT("Inventory")))
                                            ]
                                    ]
                            ]
                        // Tab content
                        + SVerticalBox::Slot()
                            .FillHeight(1.0f)
                            [
                                SAssignNew(widget_switcher, SWidgetSwitcher)
                                    .WidgetIndex(static_cast<int32>(EInGameMenuTab::Stats))
                                + SWidgetSwitcher::Slot()
                                    [
                                        SNew(SStatsTabWidget)
                                    ]
                                + SWidgetSwitcher::Slot()
                                    [
                                        SNew(SInventoryTabWidget)
                                    ]
                            ]
                    ]
            ]
    ];
    // clang-format on
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FReply SInGameMenuWidget::on_stats_tab_clicked() {
    switch_to_tab(EInGameMenuTab::Stats);
    return FReply::Handled();
}

FReply SInGameMenuWidget::on_inventory_tab_clicked() {
    switch_to_tab(EInGameMenuTab::Inventory);
    return FReply::Handled();
}

void SInGameMenuWidget::switch_to_tab(EInGameMenuTab tab) {
    if (widget_switcher.IsValid()) {
        widget_switcher->SetActiveWidgetIndex(static_cast<int32>(tab));
    }
}
