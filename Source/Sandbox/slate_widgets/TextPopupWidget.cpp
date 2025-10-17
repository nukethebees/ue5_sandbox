#include "Sandbox/slate_widgets/TextPopupWidget.h"

#include "Fonts/CompositeFont.h"
#include "SlateOptMacros.h"

#include "Sandbox/utilities/SandboxStyle.h"
#include "Sandbox/utilities/ui.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void STextPopupWidget::Construct(FArguments const& InArgs) {
    msg_ = InArgs._msg;

    // clang-format off
    ChildSlot[
        SNew(STextBlock)
            .Text(msg_)
            .TextStyle(&ml::SandboxStyle::get(), "Sandbox.Text.Widget")
    ];
    // clang-format on
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION
