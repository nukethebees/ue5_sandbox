#include "SpaceGame/ui/main_menu/LevelSelectWidget.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"
#include "SpaceGame/ui/common/MenuButtonWidget.h"

namespace ml::ioj {
void ULevelSelectWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(back_button)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("ULevelSelectWidget::NativeOnInitialized: One or more bound widgets are "
                    "invalid."));
        return;
    }

    back_button->OnClicked().AddUObject(this, &ThisClass::handle_back);
}

auto ULevelSelectWidget::NativeGetDesiredFocusTarget() const -> UWidget* {
    return back_button;
}

void ULevelSelectWidget::handle_back() {
    DeactivateWidget();
}
}
