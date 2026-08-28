#include "SpaceGame/ui/main_menu/LevelSelectWidget.h"

#include "SpaceGame/support/logging/SandboxLogCategories.h"

#include <Components/Button.h>
#include <Components/TextBlock.h>

namespace ml::ioj {
void ULevelSelectWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();

    if (!IsValid(placeholder_text) || !IsValid(back_button)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("ULevelSelectWidget::NativeOnInitialized: One or more bound widgets are "
                    "invalid."));
        return;
    }

    back_button->OnClicked.AddDynamic(this, &ThisClass::handle_back);
}

void ULevelSelectWidget::focus_back_button() {
    if (!IsValid(back_button)) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("ULevelSelectWidget::focus_back_button: Back button is invalid."));
        return;
    }

    back_button->SetKeyboardFocus();
}

void ULevelSelectWidget::handle_back() {
    back_requested.Broadcast();
}
}
