#include "Sandbox/slate_widgets/SInventoryTabWidget.h"

#include "SlateOptMacros.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#include "Sandbox/utilities/SandboxStyle.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SInventoryTabWidget::Construct(FArguments const& InArgs) {
    // clang-format off
    ChildSlot[
        SNew(SBox)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            .Padding(FMargin{20.0f})
            [
                SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
                    .Padding(FMargin{40.0f})
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Inventory content goes here")))
                            .TextStyle(&ml::SandboxStyle::get(), "Sandbox.Text.Widget")
                    ]
            ]
    ];
    // clang-format on
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION
