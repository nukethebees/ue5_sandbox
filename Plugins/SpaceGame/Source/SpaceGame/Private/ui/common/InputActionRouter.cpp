#include <SpaceGame/ui/common/InputActionRouter.h>

namespace ioj {
void UInputActionRouter::PostAnalogCursorCreate() {
    // SDL supplies pointer input. Registering the CommonUI preprocessor would turn
    // controller Accept into a second, synthetic pointer source.
}
}
