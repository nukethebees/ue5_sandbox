#include "SandboxUI/widgets/SLabeledRow.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SLabeledRow::Construct(FArguments const& args) {
    auto row{SNew(SHorizontalBox)};
    auto& label_slot{row->AddSlot()
                         .VAlign(VAlign_Center)
                         .Padding(0.0f, 0.0f, args._Spacing, 0.0f)[SNew(SBox).MinDesiredWidth(
                             args._LabelMinWidth)[SNew(STextBlock).Text(args._Label)]]};
    if (args._LabelFillWidth.IsSet()) {
        label_slot.FillWidth(args._LabelFillWidth.GetValue());
    } else {
        label_slot.AutoWidth();
    }

    row->AddSlot()
        .FillWidth(args._ContentFillWidth)
        .HAlign(args._ContentHAlign)
        .VAlign(VAlign_Center)[args._Content.Widget];

    ChildSlot[row];
}
