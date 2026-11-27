#pragma once

#include <ioj/input/flight_mapping.h>

#include <cstdint>

namespace ioj {
struct ControlBindingAddress {
    auto operator==(ControlBindingAddress const&) const -> bool = default;

    std::uint32_t profile_index{};
    FlightMapping mapping{};
    std::uint32_t slot{};
};
}
