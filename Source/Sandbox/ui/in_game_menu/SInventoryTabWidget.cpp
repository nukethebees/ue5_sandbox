#include "Sandbox/ui/in_game_menu/SInventoryTabWidget.h"

#include "SlateOptMacros.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Sandbox/ui/SandboxStyle.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SInventoryTabWidget::Construct(FArguments const& InArgs) {
    money_ = InArgs._Money;

    constexpr int32 grid_size{10};
    constexpr float cell_size{50.0f};

    TSharedRef<SGridPanel> grid_panel{SNew(SGridPanel)};

    // Create 10x10 grid
    for (int32 row{0}; row < grid_size; ++row) {
        for (int32 col{0}; col < grid_size; ++col) {
            // clang-format off
            grid_panel->AddSlot(col, row)
            [
                SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush("Border"))
                    .Padding(FMargin{1.0f})
                    [
                        SNew(SBox)
                            .WidthOverride(cell_size)
                            .HeightOverride(cell_size)
                            .HAlign(HAlign_Fill)
                            .VAlign(VAlign_Fill)
                            [
                                SNew(SBorder)
                                    .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
                            ]
                    ]
            ];
            // clang-format on
        }
    }

    // clang-format off
    ChildSlot[
        SNew(SBox)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            .Padding(FMargin{20.0f})
            [
                SNew(SHorizontalBox)
                // Grid on the left
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SScrollBox)
                        + SScrollBox::Slot()
                            [
                                grid_panel
                            ]
                    ]
                // Money display on the right
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(FMargin{20.0f, 0.0f, 0.0f, 0.0f})
                    [
                        SNew(SBorder)
                            .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
                            .Padding(FMargin{20.0f})
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .Padding(FMargin{0.0f, 0.0f, 0.0f, 0.0f})
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(TEXT("Money:")))
                                            .TextStyle(&ml::SandboxStyle::get(), "Sandbox.Text.Widget")
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .Padding(FMargin{5.0f, 0.0f, 0.0f, 0.0f})
                                    [
                                        SNew(STextBlock)
                                            .Text_Lambda([this]() {
                                                return FText::AsNumber(money_.Get());
                                            })
                                            .TextStyle(&ml::SandboxStyle::get(), "Sandbox.Text.Widget")
                                    ]
                            ]
                    ]
            ]
    ];
    // clang-format on
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION
