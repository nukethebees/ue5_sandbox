#pragma once

#include <ioj/host/modifiers.h>
#include <ioj/input/flight_input.h>

#include <ioj/input/menu_navigation.h>

#include <cstdint>
#include <span>
#include <variant>
#include <optional>
#include <string_view>

namespace ioj::host {
struct InputRoute {
    input::Context context{};
    bool movable_viewer{};
    bool resume_available{};
    bool operator==(InputRoute const&) const = default;
};

struct MenuStatus {
    bool pause_visible{};
    bool accepts_resume{};
    bool editing_text{};
    bool capturing_binding{};
    std::optional<input::MainMenuItem> main_focus;
};

enum class PlayerAction {
    OpenPause,
    RequestResume,
    BenchmarkNext,
    BenchmarkActivate,
    ToggleBenchmark,
    EndBenchmark
};
using WidgetAction = std::variant<PlayerAction, input::MainMenuItem>;

enum class PointerAction { Move, Down, Up, DoubleClick, Wheel };
struct PointerInput {
    PointerAction action;
    PhysicalKey button;
    float x;
    float y;
    float wheel;
};

struct ViewerMotion {
    float right;
    float forward;
    float up;
    float yaw;
    float pitch;
    float speed;
    bool looking;
};

#ifdef IOJ_PLAYER_EXPORTS
#define IOJ_INPUT_API __declspec(dllexport)
#else
#define IOJ_INPUT_API __declspec(dllimport)
#endif
IOJ_INPUT_API auto menu_status() -> MenuStatus;
using LegacyBindingSink = void (*)(void*, FlightMapping mapping, input::KeyCode key);
IOJ_INPUT_API void read_legacy_key_bindings(void* destination, LegacyBindingSink sink);
IOJ_INPUT_API void pause_or_exit_viewer();
IOJ_INPUT_API void present_steering(std::optional<Vector2d> steering);
IOJ_INPUT_API void present_viewer_motion(ViewerMotion const& motion, double delta_seconds);
IOJ_INPUT_API void ui_key(
    input::KeyCode code, std::uint32_t character, bool down, bool repeat, Modifiers modifiers);
IOJ_INPUT_API void ui_axis(input::KeyCode code, float value, Modifiers modifiers);
IOJ_INPUT_API void ui_pointer(PointerInput const& event, Modifiers modifiers);
IOJ_INPUT_API void ui_text(std::string_view text, Modifiers modifiers);
IOJ_INPUT_API void select_benchmark_button(bool end_selected);
IOJ_INPUT_API void focus_main_item(input::MainMenuItem item);
IOJ_INPUT_API void activate_main_item(input::MainMenuItem item);
IOJ_INPUT_API auto take_widget_actions() -> std::span<WidgetAction const>;
#undef IOJ_INPUT_API
}
