#pragma once

#include <ioj/input/flight_mapping.h>
#include <ioj/input/key_code.h>
#include <ioj/settings/controls_settings.h>

#include <cstdint>
#include <span>
#include <string_view>

namespace ioj {
enum class BindingGroup : std::uint8_t { Flight, Utility, Combat };

struct FlightControl {
    std::string_view label;
    KeyCode default_key;
    FlightMapping mapping;
    BindingGroup group;
    std::uint8_t display_order;
};
auto accepts_binding(FlightMapping mapping, KeyCode key) -> bool;
auto flight_controls() -> std::span<FlightControl const>;
auto bound_key(FlightControl const& control, ControlsSettings const& settings) -> KeyCode;
}
