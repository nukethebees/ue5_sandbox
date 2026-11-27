#pragma once

#include "player/input/logical/logical_input_system.h"
#include "player/input/physical/physical_input_system.h"
#include <ioj/input/dispatch/input_dispatcher.h>

#include <span>

namespace ioj {
struct InputRouting {
    InputRoute route;
    MenuInputMode menu_mode;
};

struct InputReceiver {
  protected:
    virtual ~InputReceiver();
  public:
    virtual auto routing() const -> InputRouting = 0;
    virtual void consume(std::span<host::MenuEvent const> events) = 0;
    virtual void consume(std::span<host::PresentationEvent const> events) = 0;
};

class InputSystem {
  public:
    InputSystem(SDL_Window& window, ControlsSettings const& settings, KeyboardLayout& layout);
    auto advance(double delta_seconds, InputReceiver& receiver) -> WindowInputEvents;
    void refresh_bindings(InputReceiver& receiver);

    // Frame views borrow dispatcher storage until the next advance or binding refresh.
    auto ship_input() const -> sim::player::PlayerInputFrame { return dispatch_.ship_input(); }
    auto menu_input() const -> std::span<host::MenuEvent const> { return dispatch_.menu_input(); }
    auto presentation_input() const -> std::span<host::PresentationEvent const> {
        return dispatch_.presentation_input();
    }

  private:
    void synchronize(InputReceiver& receiver);

    PhysicalInputSystem physical_;
    LogicalInputSystem logical_;
    InputDispatcher dispatch_;
};
}
