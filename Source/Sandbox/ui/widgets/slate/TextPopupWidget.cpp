#include "Sandbox/ui/widgets/slate/TextPopupWidget.h"

#include "Fonts/CompositeFont.h"
#include "Misc/Optional.h"
#include "SlateOptMacros.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#include "Sandbox/ui/styles/SandboxStyle.h"
#include "Sandbox/ui/utilities/ui.h"
#include "Sandbox/utilities/macros/null_checks.hpp"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void STextPopupWidget::Construct(FArguments const& InArgs) {
    msg_ = InArgs._msg;
    on_dismissed_ = InArgs._OnDismissed;

    // clang-format off
    ChildSlot[
        SNew(SBox)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
            SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
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
        ]
    ];
    // clang-format on
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FReply STextPopupWidget::OnMouseButtonDown(FGeometry const& MyGeometry,
                                           FPointerEvent const& MouseEvent) {
    constexpr auto logger{NestedLogger<"OnMouseButtonDown">()};

    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) {
        drag_mouse_pos_begin_ = MouseEvent.GetScreenSpacePosition();
        auto xform{GetRenderTransform().Get({})};
        drag_translation_begin_ = xform.GetTranslation();
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    return FReply::Unhandled();
}
FReply STextPopupWidget::OnMouseMove(FGeometry const& MyGeometry, FPointerEvent const& MouseEvent) {
    constexpr auto logger{NestedLogger<"OnMouseMove">()};

    if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && HasMouseCapture()) {
        auto const& abs_mouse_pos{MouseEvent.GetScreenSpacePosition()};
        auto const local_mouse_cur{MyGeometry.AbsoluteToLocal(abs_mouse_pos)};
        auto const local_mouse_start{MyGeometry.AbsoluteToLocal(drag_mouse_pos_begin_)};
        auto const local_mouse_delta{local_mouse_cur - local_mouse_start};

        FTransform2D xform{GetRenderTransform().Get({})};
        SetRenderTransform(drag_translation_begin_ + local_mouse_delta);

        return FReply::Handled();
    }

    return FReply::Unhandled();
}
FReply STextPopupWidget::OnMouseButtonUp(FGeometry const& MyGeometry,
                                         FPointerEvent const& MouseEvent) {
    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) {
        return FReply::Handled().ReleaseMouseCapture();
    }
    return FReply::Unhandled();
}
