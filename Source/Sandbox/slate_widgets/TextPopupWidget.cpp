#include "Sandbox/slate_widgets/TextPopupWidget.h"

#include "Fonts/CompositeFont.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#include "Sandbox/utilities/SandboxStyle.h"
#include "Sandbox/utilities/ui.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void STextPopupWidget::Construct(FArguments const& InArgs) {
    msg_ = InArgs._msg;
    on_dismissed_ = InArgs._OnDismissed;

    // clang-format off
    ChildSlot[
        SNew(SBox)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            .Padding(FMargin{20.0f})
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin{0.0f, 0.0f, 0.0f, 10.0f})
                    [
                        SNew(STextBlock)
                            .Text(msg_)
                            .TextStyle(&ml::SandboxStyle::get(), "Sandbox.Text.Widget")
                    ]
                + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Center)
                    [
                        SNew(SButton)
                            .OnClicked(on_dismissed_)
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("OK")))
                            ]
                    ]
            ]
    ];
    // clang-format on
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION
