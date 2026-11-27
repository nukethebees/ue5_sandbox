#pragma once

#include <ioj/input/actions.h>
#include <ioj/input/flight_mapping.h>
#include <ioj/input/physical_key.h>

#include <span>
#include <string_view>

namespace ioj {
struct FlightControl {
    std::string_view label;
    PhysicalKey default_key;
    FlightMapping mapping;
    BindingGroup group;
};
auto accepts_binding(FlightMapping mapping, PhysicalKey key) -> bool;
auto flight_controls() -> std::span<FlightControl const>;
}
