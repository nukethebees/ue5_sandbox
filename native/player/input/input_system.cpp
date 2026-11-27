#include "player/input/input_system.h"

namespace ioj {
InputReceiver::~InputReceiver() = default;

InputSystem::InputSystem(SDL_Window& window,
                         ControlsSettings const& settings,
                         KeyboardLayout& layout)
    : physical_{window, layout}
    , logical_{settings, physical_.state()} {}

void InputSystem::synchronize(InputReceiver& receiver) {
    for (;;) {
        auto const first{dispatch_.size()};
        auto const [route, mode]{receiver.routing()};
        logical_.synchronize(dispatch_, route, mode);
        if (dispatch_.size() == first) {
            break;
        }
        dispatch_.deliver_from(first, receiver);
    }
    physical_.set_window_modes(logical_.relative_mouse(), logical_.editing_text());
}

auto InputSystem::advance(double delta_seconds, InputReceiver& receiver) -> WindowInputEvents {
    dispatch_.begin_frame();
    logical_.begin_frame();
    synchronize(receiver);

    auto const window{physical_.poll()};
    auto const& batch{physical_.events()};
    auto const count{batch.order.size()};
    for (std::size_t cursor{}; cursor < count;) {
        auto const first{dispatch_.size()};
        cursor = logical_.map_until_dispatch(dispatch_, batch, cursor);
        dispatch_.deliver_from(first, receiver);
        synchronize(receiver);
    }

    auto const first{dispatch_.size()};
    if (physical_.focused()) {
        logical_.repeat_navigation(dispatch_, delta_seconds);
    }
    dispatch_.deliver_from(first, receiver);
    dispatch_.publish_ship(logical_.state());
    return window;
}
void InputSystem::refresh_bindings(InputReceiver& receiver) {
    auto const first{dispatch_.size()};
    logical_.refresh_bindings(dispatch_);
    dispatch_.deliver_from(first, receiver);
    synchronize(receiver);
    dispatch_.publish_ship(logical_.state());
}
}
