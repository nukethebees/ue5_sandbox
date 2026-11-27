#include "SpaceGame/ui/common/MenuActivatableWidget.h"

#include <InputCoreTypes.h>

namespace ioj {
UMenuActivatableWidget::UMenuActivatableWidget() {
    bIsBackHandler = false;
    bAutoRestoreFocus = true;
}

FReply UMenuActivatableWidget::NativeOnKeyDown(FGeometry const& geometry, FKeyEvent const& event) {
    if ((event.GetKey() == EKeys::Escape || event.GetKey() == EKeys::Gamepad_FaceButton_Right) &&
        !event.IsRepeat() && NativeOnHandleBackAction()) {
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(geometry, event);
}

TOptional<FUIInputConfig> UMenuActivatableWidget::GetDesiredInputConfig() const {
    auto config{FUIInputConfig{
        ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, EMouseLockMode::DoNotLock, false}};
    config.bIgnoreMoveInput = true;
    config.bIgnoreLookInput = true;
    return config;
}
}
