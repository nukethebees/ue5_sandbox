#pragma once
#include <ioj/host/input.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

#include <array>
#include <cstddef>
#include <string_view>

struct SDL_Window;

namespace ioj {
class SdlInput {
  public:
    SdlInput(SDL_Window& window, ControlsSettings const& controls);
    ~SdlInput();
    SdlInput(SdlInput const&) = delete;
    auto operator=(SdlInput const&) -> SdlInput& = delete;

    void begin_frame();
    void handle(SDL_Event const& event);
    void finish_frame(double dt);
    auto ship_frame() const { return flight_.frame(); }
  private:
    void synchronize();
    void key(std::string_view name, bool down, bool repeat, Uint32 character);
    void axis_value(std::size_t axis, float value);
    void release_gamepad();

    SDL_Window& window_;
    ControlsSettings const& controls_;
    SDL_Gamepad* gamepad_{};
    host::InputRoute route_{};
    host::Modifiers modifiers_{};
    input::FlightInput flight_;
    Uint32 ui_mouse_buttons_{};
    std::array<float, SDL_GAMEPAD_AXIS_COUNT> axes_{};
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> pad_buttons_{};
    std::array<bool, 7> viewer_buttons_{};
    int speed_index_{2};
    std::array<double, 4> dpad_repeat_{};
    bool viewer_looking_{};
    float viewer_dx_{};
    float viewer_dy_{};
};
}
