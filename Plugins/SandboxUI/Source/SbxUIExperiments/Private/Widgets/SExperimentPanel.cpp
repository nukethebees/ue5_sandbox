#include "Widgets/SExperimentPanel.h"

#include "SandboxUI/slate/SlateSlots.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SExperimentPanel::Construct(FArguments const& args) {
    ChildSlot[SNew(SBorder)
                  .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                  .Padding(
                      12.0f)[SNew(SVerticalBox) +
                             SandboxUI::Slate::vbox_auto_slot(FMargin{0.0f, 0.0f, 0.0f, 4.0f})
                                 [SNew(STextBlock)
                                      .Text(args._Title)
                                      .Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))] +
                             SandboxUI::Slate::vbox_auto_slot(FMargin{0.0f, 0.0f, 0.0f, 10.0f})
                                 [SNew(STextBlock).Text(args._Description)] +
                             SandboxUI::Slate::vbox_auto_slot(
                                 FMargin{0.0f, 0.0f, 0.0f, 10.0f})[args._Controls.Widget] +
                             SandboxUI::Slate::vbox_fill_slot()
                                 .HAlign(HAlign_Center)
                                 .VAlign(VAlign_Center)[args._Preview.Widget]]];
}

void SExperimentBenchmark::Construct(FArguments const& args) {
    ChildSlot[SNew(SVerticalBox) +
              SandboxUI::Slate::vbox_auto_slot()[SNew(SButton)
                                                     .Text(args._ButtonText)
                                                     .ToolTipText(args._ToolTipText)
                                                     .OnClicked(args._OnClicked)] +
              SandboxUI::Slate::vbox_auto_slot(FMargin{0.0f, 4.0f, 0.0f, 0.0f})
                  [SNew(SBox).HeightOverride(args._OutputHeight)[args._Output.Widget]]];
}
