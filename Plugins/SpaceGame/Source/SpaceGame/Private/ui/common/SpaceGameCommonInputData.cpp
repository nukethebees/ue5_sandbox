#include "SpaceGame/ui/common/SpaceGameCommonInputData.h"

#include <InputAction.h>
#include <UObject/ConstructorHelpers.h>

namespace ml::ioj {
USpaceGameCommonInputData::USpaceGameCommonInputData() {
    static ConstructorHelpers::FObjectFinder<UInputAction> const back_action{
        TEXT("/SpaceGame/Input/UI/IA_menu_back")};
    EnhancedInputBackAction = back_action.Object;
}
}
