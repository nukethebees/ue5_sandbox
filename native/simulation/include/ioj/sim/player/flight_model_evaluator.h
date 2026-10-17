#pragma once

#include <ioj/sim/player/flight_model_config.h>
#include <ioj/sim/player/flight_model_controller_state.h>
#include <ioj/sim/player/physical_movement_state.h>
#include <ioj/sim/player/player_flight_intent.h>
#include <ioj/sim/player/player_presentation_state.h>
#include <ioj/sim/player/player_resource_state.h>
#include <ioj/sim/player/player_simulation_state.h>

namespace ioj::sim::player {
void seed_flight_model_responses(PlayerSimulationState& state,
                                 FlightModelConfig const& config) noexcept;
void prepare_flight_model_action_transition(PlayerSimulationState& state,
                                            FlightModelConfig const& config) noexcept;
void clamp_flight_model_persistent_targets(PlayerSimulationState& state,
                                           FlightModelConfig const& config) noexcept;
void reset_flight_model_controller(PlayerSimulationState& state,
                                   FlightModelConfig const& config,
                                   bool clear_persistent_targets = true) noexcept;
void integrate_flight_model(float dt,
                            FlightModelConfig const& config,
                            PlayerFlightIntent const& intent,
                            PlayerSimulationState& state) noexcept;
}
