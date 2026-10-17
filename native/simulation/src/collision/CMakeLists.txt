target_sources(native-simulation PRIVATE
  cell_coord.cpp
  grid_traversal.cpp
  ../../include/ioj/sim/collision/grid_traversal.h
  ../../include/ioj/sim/collision/collision_system.h
  ../../include/ioj/sim/collision/collision_uniform_grid.h
  collision_system.cpp
  collision_uniform_grid.cpp
)
