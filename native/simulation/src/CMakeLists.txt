set(simulation_headers
  batch_operations.h
  combat_events.h
  combat_statistics.h
  damage_queue.h
  deterministic_bias.h
  direct_damage_events.h
  fixed_tick.h
  fixed_tick_loop.h
  index_span.h
  overlap_handler.h
  profiling.h
  sim_clock.h
  sim_tick.h
  sim_time.h
  step_response.h
)

list(TRANSFORM simulation_headers PREPEND "../include/ioj/sim/")

set(capital_ships_headers
  capital_entity_data.h
)

list(TRANSFORM capital_ships_headers PREPEND "../include/ioj/sim/capital_ships/")

set(collision_headers
  cell_coords.h
  collision_events.h
  collision_grid.h
  collision_grid_entity_storage.h
  collision_grid_static_storage.h
  collision_types.h
  entity_aabbs.h
  entity_cell_data.h
  entity_overlaps.h
  entity_world_bounds.h
  line_trace_batch.h
  line_traces.h
  range_query_results.h
  spatial.h
  spatial_query_manager.h
  trace_hits.h
  world_aabbs.h
)

list(TRANSFORM collision_headers PREPEND "../include/ioj/sim/collision/")

set(entities_headers
  entity_death_info.h
  entity_frame_change.h
  entity_history.h
  entity_id.h
  entity_id_checks.h
  entity_ledger.h
  entity_life_state.h
  entity_lookup_table.h
  entity_table_operations.h
  entity_tables.h
  entity_telemetry.h
  entity_type.h
  entity_type_runs.h
  health_table.h
  ship_health.h
  team_checks.h
  team_entity_array.h
)

list(TRANSFORM entities_headers PREPEND "../include/ioj/sim/entities/")

set(expressions_headers
  expr_manager.h
  expr_compilation_context.h
  expr_evaluation_context.h
  expr_leaves.h
  expr_operators.h
  expr_handle.h
  expr_kind.h
  expr_composite_index.h
  composite_kind.h
  leaf_handle.h
  leaf_data_handle.h
  leaf_index.h
  leaf_data_index.h
  leaf_kind.h
  kill_count.h
)

list(TRANSFORM expressions_headers PREPEND "../include/ioj/sim/expressions/")

set(missions_headers
  mission.h
  mission_changes.h
  mission_evaluation_context.h
  objectives.h
)

list(TRANSFORM missions_headers PREPEND "../include/ioj/sim/missions/")

set(fighters_headers
  fighter_entity_data.h
  fighter_navigation.h
  fighter_navigation_scratch.h
  navigation_counters.h
  fighter_spawn_queue.h
)

list(TRANSFORM fighters_headers PREPEND "../include/ioj/sim/fighters/")

set(lasers_headers
  laser_entities.h
  laser_frame_output.h
  laser_hit_details.h
  laser_source.h
  laser_spawn_requests.h
)

list(TRANSFORM lasers_headers PREPEND "../include/ioj/sim/lasers/")

set(levels_headers
  level_event_types.h
  level_memory_manager.h
  level_read_access.h
  level_sim.h
  level_state.h
  spawn_context.h
  reference_level_simulation_data.h
  tick_player_ship_state.h
)

list(TRANSFORM levels_headers PREPEND "../include/ioj/sim/levels/")

set(player_headers
  player_spatial_data.h
  ship_flight_model.h
)

list(TRANSFORM player_headers PREPEND "../include/ioj/sim/player/")

set(spinners_headers
  spinner_entity_data.h
)

list(TRANSFORM spinners_headers PREPEND "../include/ioj/sim/spinners/")

set(tasks_headers
  executor_table.h
  instruction.h
  instruction_data.h
  instruction_handle.h
  task_allocation_context.h
  task_entity_count.h
  task_id.h
  task_manager.h
  task_planning_context.h
)

list(TRANSFORM tasks_headers PREPEND "../include/ioj/sim/tasks/")

set(telemetry_headers
  battle_change_mask.h
  recording_config.h
  level_telemetry_manager.h
  run_data.h
  snapshot.h
)

list(TRANSFORM telemetry_headers PREPEND "../include/ioj/sim/telemetry/")

set(testing_headers
  laser_spawns.h
  level_sim_test_access.h
  spatial_query_manager_test_access.h
)

list(TRANSFORM testing_headers PREPEND "../include/ioj/sim/testing/")

set(turrets_headers
  turret_entity_data.h
)

list(TRANSFORM turrets_headers PREPEND "../include/ioj/sim/turrets/")

target_sources(native-simulation
  PRIVATE
    entities/entity_ledger.cpp
    damage_queue.cpp
    batch_operations.cpp
    collision/collision_events.cpp
    collision/collision_grid_static_storage.cpp
    collision/collision_grid.cpp
    deterministic_bias.cpp
    combat_statistics.cpp
    collision/entity_world_bounds.cpp
    fighters/fighter_navigation.cpp
    fixed_tick.cpp
    lasers/laser_frame_output.cpp
    levels/level_memory_manager.cpp
    levels/level_read_access.cpp
    levels/level_sim.cpp
    telemetry/level_telemetry_manager.cpp
    expressions/expr_manager.cpp
    missions/mission.cpp
    missions/objectives.cpp
    overlap_handler.cpp
    levels/reference_level_simulation_data.cpp
    player/ship_flight_model.cpp
    sim_time.cpp
    collision/spatial_query_manager.cpp
    collision/spatial_range_queries.cpp
    collision/spatial.cpp
    step_response.cpp
    tasks/task_manager.cpp
    tasks/executor_table.cpp

    ${simulation_headers}
    ${capital_ships_headers}
    ${collision_headers}
    ${entities_headers}
    ${expressions_headers}
    ${missions_headers}
    ${fighters_headers}
    ${lasers_headers}
    ${levels_headers}
    ${player_headers}
    ${spinners_headers}
    ${tasks_headers}
    ${telemetry_headers}
    ${testing_headers}
    ${turrets_headers}
)

add_subdirectory(capital_ships)
add_subdirectory(collision)
add_subdirectory(fighters)
add_subdirectory(lasers)
add_subdirectory(levels)
add_subdirectory(memory)
add_subdirectory(player)
add_subdirectory(spinners)
add_subdirectory(telemetry)
add_subdirectory(turrets)
