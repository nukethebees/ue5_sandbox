#include "SSandboxEditorToolsMainPanel.h"

#include "Widgets/Text/STextBlock.h"

void SSandboxEditorToolsMainPanel::Construct(FArguments const& in_args) {
    // clang-format off
    ChildSlot
    [
        SNew(STextBlock)
        .Text(FText::FromString("Hello Editor"))
    ];
    // clang-format on
}
