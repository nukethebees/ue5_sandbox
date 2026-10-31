#pragma once

#include <ioj/sim/player/flight_model_config.h>

#include <array>
#include <string>
#include <string_view>

namespace ioj::flight_model_settings_codec {
auto encode(sim::player::FlightModelProfile const& profile) -> std::string;
// Invalid, unsupported or truncated values leave the profile unchanged.
auto decode(std::string_view data, sim::player::FlightModelProfile& profile) -> bool;

struct DecodedLoadout {
    sim::player::FlightModelLoadout value;
    std::array<bool, 4> invalid_slots{};
};

auto decode_loadout(std::array<std::string_view, 4> const& data) -> DecodedLoadout;
}
