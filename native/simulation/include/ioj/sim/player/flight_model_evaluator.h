#pragma once

#include <ioj/sim/player/flight_model_config.h>
#include <ioj/sim/player/flight_model_runtime.h>

namespace ioj::sim::player {
void seed_flight_model_responses(PlayerSimulationState& state,
                                 FlightModelConfig const& config) noexcept;
void prepare_flight_model_action_transition(PlayerSimulationState& state,
                                            FlightModelConfig const& config) noexcept;
void reset_flight_model_controller(PlayerSimulationState& state,
                                   FlightModelConfig const& config) noexcept;
void integrate_flight_model(float dt,
                            FlightModelConfig const& config,
                            PlayerFlightIntent const& intent,
                            PlayerSimulationState& state) noexcept;
}
