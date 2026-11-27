#include <ioj/input/input_state.h>

namespace ioj::input {
void InputState::button(DigitalAction action, ButtonState edge) {
    if (edge == ButtonState::Repeated && !held.get(action)) {
        return;
    }
    if (edge != ButtonState::Repeated) {
        auto const down{edge == ButtonState::Pressed};
        if (held.get(action) == down) {
            return;
        }
        held.set(action, down);
        (down ? pressed : released).set(action, true);
    }
    transitions.push_back({action, edge});
}

void InputState::begin_frame() {
    transitions.clear();
    pressed = {};
    released = {};
    axes_2d[Axis2DAction::ObserverLookDelta] = {};
}
}
