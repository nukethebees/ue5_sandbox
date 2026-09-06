#include "SpaceGame/ui/main_menu/LevelSelectWidget.h"

namespace ml::ioj {
ULevelSelectWidget::ULevelSelectWidget(FObjectInitializer const& object_initializer)
    : Super(object_initializer) {
    SetIsFocusable(true);
}

void ULevelSelectWidget::prepare_for_open(FName const preferred_level_id) noexcept {
    preferred_level_id_ = preferred_level_id;
}

}
