#pragma once

#include "player/input/physical/physical_input.h"
#include "player/input/physical/physical_input_batch.h"
#include <ioj/input/dispatch/player_menu.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

struct SDL_Window;

namespace ioj {
struct WindowInputEvents {
    bool close_requested{};
    bool viewport_changed{};
    bool display_changed{};
};

class PhysicalInputSystem {
  public:
    PhysicalInputSystem(SDL_Window& window, KeyboardLayout& layout);
    ~PhysicalInputSystem();
    PhysicalInputSystem(PhysicalInputSystem const&) = delete;
    auto operator=(PhysicalInputSystem const&) -> PhysicalInputSystem& = delete;

    void set_window_modes(bool relative_mouse, bool text_input);
    auto poll() -> WindowInputEvents;
    auto events() const -> PhysicalInputBatch const& { return events_; }
    auto state() -> PhysicalInputState& { return state_; }
    auto focused() const -> bool;
  private:
    void collect(SDL_Event const& event);

    SDL_Window& window_;
    KeyboardLayout& layout_;
    PhysicalInputBatch events_;
    PhysicalInputState state_;
    SDL_Gamepad* gamepad_{};
};
}
