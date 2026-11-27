#pragma once

#include <ioj/enum_array.h>
#include <ioj/input/flight_mapping.h>
#include <ioj/input/physical_key.h>

namespace ioj {
using FlightBindings = EnumArray<PhysicalKey, FlightMapping>;

auto default_flight_bindings() -> FlightBindings;
}
