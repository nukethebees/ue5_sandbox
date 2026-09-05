#include "SpaceGame/ui/main_menu/LevelSelectWidget.h"

namespace ml::ioj {
ULevelSelectWidget::ULevelSelectWidget() {
    SetIsFocusable(true);
}

void ULevelSelectWidget::prepare_for_open(FName const preferred_level_id) noexcept {
    preferred_level_id_ = preferred_level_id;
}

auto ULevelSelectWidget::NativeGetDesiredFocusTarget() const -> UWidget* {
    return const_cast<ULevelSelectWidget*>(this);
}

auto ULevelSelectWidget::NativeOnHandleBackAction() -> bool {
    DeactivateWidget();
    return true;
}
}
