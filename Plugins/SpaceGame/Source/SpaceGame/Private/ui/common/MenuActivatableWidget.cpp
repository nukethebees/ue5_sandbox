#include "SpaceGame/ui/common/MenuActivatableWidget.h"

namespace ioj {
void UMenuActivatableWidget::close() {
    RemoveFromParent();
    closed.Broadcast();
}

}
