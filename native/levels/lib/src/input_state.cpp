#include <ioj/input/logical/input_state.h>

namespace ioj {
void LogicalInputState::button(DigitalAction action, ButtonState edge) {
    auto const down{edge == ButtonState::Pressed};
    if (held.get(action) == down) {
        return;
    }
    held.set(action, down);
    transitions.push_back({action, edge});
}

void LogicalInputState::begin_frame() {
    transitions.clear();
    axes_2d[Axis2DAction::ObserverLookDelta] = {};
}
}
