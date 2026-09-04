#include "SandboxUI/widgets/SValueSlider.h"

#include "SandboxUI/slate/SlateSlots.h"
#include "SandboxUI/widgets/SLabeledRow.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "generated/SValueSlider.slate.generated.h"

void SValueSlider::Construct(FArguments const& args) {
    auto const value_text_padding{FMargin{args._ValueTextSpacing, 0.0f}};
    ChildSlot[SlateGenerated::SValueSliderBuilder{*this}.Build(args._Label,
                                                               args._LabelFillWidth,
                                                               args._ControlFillWidth,
                                                               args._LabelSpacing,
                                                               args._Value,
                                                               args._OnValueChanged,
                                                               value_text_padding,
                                                               args._ValueTextMinWidth,
                                                               args._ValueText)];
}
