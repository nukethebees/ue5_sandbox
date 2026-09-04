#include "SandboxUI/widgets/SLabeledRow.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SLabeledRow::Construct(FArguments const& args) {
    ChildSlot[SNew(SHorizontalBox) +
              SHorizontalBox::Slot()
                  .AutoWidth()
                  .VAlign(VAlign_Center)
                  .Padding(0.0f, 0.0f, args._Spacing, 0.0f)[SNew(SBox).MinDesiredWidth(
                      args._LabelMinWidth)[SNew(STextBlock).Text(args._Label)]] +
              SHorizontalBox::Slot()
                  .FillWidth(1.0f)
                  .HAlign(args._ContentHAlign)
                  .VAlign(VAlign_Center)[args._Content.Widget]];
}
