#pragma once
#include <ioj/host/input.h>
#include <ioj/input/ship_input.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

struct SDL_Window;

namespace ioj {
class SdlInput {
  public:
    SdlInput(SDL_Window& window, ControlsSettings const& controls);
    ~SdlInput();
    SdlInput(SdlInput const&) = delete;
    auto operator=(SdlInput const&) -> SdlInput& = delete;

    void synchronize(host::InputRoute current, host::MenuStatus menu);
    void refresh_bindings();
    void begin_frame();
    void handle(SDL_Event const& event);
    void finish_frame(double dt);
    auto events() const -> std::span<host::InputEvent const> { return events_; }
    auto ship_frame() { return input::ship_input_frame(state_, ship_events_); }
  private:
    void key(input::KeyCode code, bool down, bool repeat, Uint32 character);
    void axis_value(std::size_t axis, float value);
    void release_gamepad();

    SDL_Window& window_;
    ControlsSettings const& controls_;
    SDL_Gamepad* gamepad_{};
    host::InputRoute route_{};
    host::Modifiers modifiers_{};
    bool raw_menu_input_{};
    input::FlightInputResolver flight_;
    input::InputState state_;
    std::vector<sim::player::PlayerInputKind> ship_events_;
    std::vector<host::InputEvent> events_;
    Uint32 ui_mouse_buttons_{};
    std::array<float, SDL_GAMEPAD_AXIS_COUNT> axes_{};
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> pad_buttons_{};
    std::array<bool, 7> viewer_buttons_{};
    int speed_index_{2};
    std::array<double, 8> direction_repeat_{};
    bool viewer_looking_{};
    float viewer_dx_{};
    float viewer_dy_{};
};
}
