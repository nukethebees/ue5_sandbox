#include "SpaceGame/ui/common/MenuActivatableWidget.h"

#include <Input/UIActionBindingHandle.h>
#include <InputMappingContext.h>
#include <UObject/ConstructorHelpers.h>

namespace ml::ioj {
UMenuActivatableWidget::UMenuActivatableWidget() {
    bIsBackHandler = true;
    bAutoRestoreFocus = true;

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> const menu_mapping{
        TEXT("/SpaceGame/Input/UI/IMC_menu")};
    InputMapping = menu_mapping.Object;
    InputMappingPriority = 1000;
}

TOptional<FUIInputConfig> UMenuActivatableWidget::GetDesiredInputConfig() const {
    auto config{FUIInputConfig{
        ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, EMouseLockMode::DoNotLock, false}};
    config.bIgnoreMoveInput = true;
    config.bIgnoreLookInput = true;
    return config;
}
}
