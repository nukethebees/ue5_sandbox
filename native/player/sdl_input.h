#pragma once

#include "player/physical_input.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

#include <optional>

struct SDL_Window;

namespace ioj {
class SdlInput {
  public:
    explicit SdlInput(SDL_Window& window);
    ~SdlInput();
    SdlInput(SdlInput const&) = delete;
    auto operator=(SdlInput const&) -> SdlInput& = delete;

    void set_window_modes(bool relative_mouse, bool text_input);
    auto translate(SDL_Event const& event) -> std::optional<input::PhysicalEvent>;
    auto focused() const -> bool;
  private:
    SDL_Window& window_;
    SDL_Gamepad* gamepad_{};
};
}
