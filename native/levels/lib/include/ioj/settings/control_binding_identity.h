#pragma once

#include <ioj/input/flight_mapping.h>

#include <cstdint>

namespace ioj {
struct ControlBindingIdentity {
    auto operator==(ControlBindingIdentity const&) const -> bool = default;

    FlightMapping mapping{};
    std::uint32_t slot{};
    std::uint32_t device_type{};
};
}
