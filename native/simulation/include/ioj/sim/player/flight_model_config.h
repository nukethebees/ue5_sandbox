#pragma once

#include <ioj/sim/player/flight_model_data.h>
#include <ioj/sim/player/flight_model_enums.h>

#include <expected>

namespace ioj::sim::player {

[[nodiscard]] auto validate_flight_model_config(FlightModelConfig const& config) noexcept
    -> std::expected<void, FlightModelConfigError>;
[[nodiscard]] auto make_flight_model_profile(FlightModelPreset preset) -> FlightModelProfile;
[[nodiscard]] auto matches_authored_flight_model_topology(FlightModelConfig const& config,
                                                          FlightModelPreset preset) -> bool;
[[nodiscard]] auto make_default_flight_model_loadout() -> FlightModelLoadout;
[[nodiscard]] auto flight_model_profile(FlightModelLoadout& loadout, FlightModelSlot slot) noexcept
    -> FlightModelProfile&;
[[nodiscard]] auto flight_model_profile(FlightModelLoadout const& loadout,
                                        FlightModelSlot slot) noexcept -> FlightModelProfile const&;
}
