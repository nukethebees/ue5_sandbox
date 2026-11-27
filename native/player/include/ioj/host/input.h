#pragma once

#include <ioj/host/modifiers.h>
#include <ioj/input/flight_input.h>
#include <ioj/input/menu_navigation.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

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

struct MenuButton {
    input::MenuCommand command;
    bool down;
    bool repeat;
    Modifiers modifiers;
};
struct MenuKey {
    input::KeyCode key;
    std::uint32_t character{};
    bool down{};
    bool repeat{};
    Modifiers modifiers{};
};
struct MenuAxis {
    input::KeyCode key;
    float value;
    Modifiers modifiers;
};
struct MenuPointer {
    PointerInput pointer;
    Modifiers modifiers;
};
struct MenuText {
    std::string text;
    Modifiers modifiers;
};
struct SteeringIndicator {
    std::optional<Vector2d> direction;
};
struct ViewerStep {
    ViewerMotion motion;
    double seconds;
};
using PresentationEvent =
    std::variant<MenuKey, MenuAxis, MenuPointer, MenuText, SteeringIndicator, ViewerStep>;
using WidgetAction = std::variant<PlayerAction, input::MainMenuItem>;
using InputEvent = std::variant<PlayerAction, MenuButton, PresentationEvent>;

#ifdef IOJ_PLAYER_EXPORTS
#define IOJ_INPUT_API __declspec(dllexport)
#else
#define IOJ_INPUT_API __declspec(dllimport)
#endif
IOJ_INPUT_API auto menu_status() -> MenuStatus;
using LegacyBindingSink = void (*)(void*, FlightMapping mapping, input::KeyCode key);
IOJ_INPUT_API void read_legacy_key_bindings(void* destination, LegacyBindingSink sink);
IOJ_INPUT_API void show_pause_menu();
IOJ_INPUT_API void request_resume();
IOJ_INPUT_API void select_benchmark_button(bool end_selected);
IOJ_INPUT_API void focus_main_item(input::MainMenuItem item);
IOJ_INPUT_API void activate_main_item(input::MainMenuItem item);
IOJ_INPUT_API void present_menu_button(MenuButton button);
IOJ_INPUT_API auto take_widget_actions() -> std::span<WidgetAction const>;
IOJ_INPUT_API void present_input(PresentationEvent const& event);
#undef IOJ_INPUT_API
}
