#include "SandboxUI/widgets/SLabeledRow.h"

#include "SandboxUI/slate/SlateSlots.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "generated/SLabeledRow.slate.generated.h"

void SLabeledRow::Construct(FArguments const& args) {
    auto builder{SlateGenerated::SLabeledRowBuilder{*this}};
    auto const label_padding{FMargin{0.0f, 0.0f, args._Spacing, 0.0f}};
    if (args._LabelFillWidth.IsSet()) {
        ChildSlot[builder.BuildFillLabel(args._Label,
                                         args._LabelMinWidth,
                                         label_padding,
                                         args._LabelFillWidth.GetValue(),
                                         args._ContentFillWidth,
                                         args._ContentHAlign,
                                         args._Content.Widget)];
    } else {
        ChildSlot[builder.BuildAutoLabel(args._Label,
                                         args._LabelMinWidth,
                                         label_padding,
                                         args._ContentFillWidth,
                                         args._ContentHAlign,
                                         args._Content.Widget)];
    }
}
