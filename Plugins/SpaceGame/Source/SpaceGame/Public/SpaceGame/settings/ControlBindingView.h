#pragma once

#include <ioj/input/logical/flight_bindings.h>

#include <Internationalization/Text.h>

namespace ioj {
struct FControlBindingView {
    auto is_modified() const -> bool { return current_key != default_key; }

    FlightMapping mapping;
    FText display_name;
    FText display_category;
    PhysicalKey current_key;
    PhysicalKey default_key;
};
}
