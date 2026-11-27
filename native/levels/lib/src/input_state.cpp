#include <ioj/input/input_state.h>

namespace ioj {
void InputState::button(DigitalAction action, ButtonState edge) {
    auto const down{edge == ButtonState::Pressed};
    if (held.get(action) == down) {
        return;
    }
    held.set(action, down);
    transitions.push_back({action, edge});
}

void InputState::begin_frame() {
    transitions.clear();
    axes_2d[Axis2DAction::ObserverLookDelta] = {};
}
}
