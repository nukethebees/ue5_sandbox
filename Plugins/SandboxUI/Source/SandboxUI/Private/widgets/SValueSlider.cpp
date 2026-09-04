#include "SandboxUI/widgets/SValueSlider.h"

#include "SandboxUI/slate/SlateSlots.h"
#include "SandboxUI/widgets/SLabeledRow.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SValueSlider::Construct(FArguments const& args) {
    ChildSlot[SNew(SLabeledRow)
                  .Label(args._Label)
                  .LabelFillWidth(args._LabelFillWidth)
                  .ContentFillWidth(args._ControlFillWidth)
                  .Spacing(args._LabelSpacing)
                      [SNew(SHorizontalBox) +
                       SandboxUI::Slate::hbox_fill_slot()
                           [SNew(SSlider).Value(args._Value).OnValueChanged(args._OnValueChanged)] +
                       SandboxUI::Slate::hbox_auto_slot(
                           FMargin{args._ValueTextSpacing, 0.0f})[SNew(SBox).MinDesiredWidth(
                           args._ValueTextMinWidth)[SNew(STextBlock).Text(args._ValueText)]]]];
}
