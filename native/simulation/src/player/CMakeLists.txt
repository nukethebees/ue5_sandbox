target_sources(native-simulation PRIVATE
  ../../include/ioj/sim/player/control_mode.h
  ../../include/ioj/sim/player/command_interface.h
  ../../include/ioj/sim/player/fire_rate.h
  ../../include/ioj/sim/player/flight_mode.h
  ../../include/ioj/sim/player/laser_firing_state.h
  ../../include/ioj/sim/player/phase_interface.h
  ../../include/ioj/sim/player/player_read_view.h
  ../../include/ioj/sim/player/ship_laser_mode.h
  ../../include/ioj/sim/player/sim.h
  ../../include/ioj/sim/player/space_ship_common.h
  phase_interface.cpp
  sim.cpp
)
