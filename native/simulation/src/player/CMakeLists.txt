target_sources(native-simulation PRIVATE
  ../../include/ioj/sim/player/physical_movement_state.h
  ../../include/ioj/sim/player/player_flight_intent.h
  ../../include/ioj/sim/player/player_resource_state.h
  ../../include/ioj/sim/player/player_presentation_state.h
  ../../include/ioj/sim/player/player_simulation_state.h
  ../../include/ioj/sim/player/laser_firing_state.h
  ../../include/ioj/sim/player/phase_interface.h
  ../../include/ioj/sim/player/sim.h
  ../../include/ioj/sim/player/space_ship_common.h
  game_settings_session.cpp
  sim.cpp
)
