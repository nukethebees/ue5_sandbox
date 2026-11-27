#include "SpaceGame/ui/common/MenuActivatableWidget.h"

#include <InputCoreTypes.h>

namespace ioj {
void UMenuActivatableWidget::close() {
    RemoveFromParent();
    closed.Broadcast();
}

FReply UMenuActivatableWidget::NativeOnKeyDown(FGeometry const& geometry, FKeyEvent const& event) {
    if ((event.GetKey() == EKeys::Escape || event.GetKey() == EKeys::Gamepad_FaceButton_Right) &&
        !event.IsRepeat() && handle_back()) {
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(geometry, event);
}

}
