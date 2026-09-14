add_library(native-simulation STATIC)
target_sources(native-simulation PRIVATE
  include/ioj/sim/attack_distance_band.h
  include/ioj/sim/batch_operations.h
  include/ioj/sim/capital_death_event.h
  include/ioj/sim/capital_entity_data.h
  include/ioj/sim/capital_ships/phase_interface.h
  include/ioj/sim/capital_ships/sim.h
  include/ioj/sim/capital_ships/spawn_data.h
  include/ioj/sim/capital_spawn_data.h
  include/ioj/sim/collision_events.h
  include/ioj/sim/collision_grid_entity_storage.h
  include/ioj/sim/collision_grid_static_storage.h
  include/ioj/sim/collision_grid.h
  include/ioj/sim/collision_overlap_storage.h
  include/ioj/sim/collision/collision_system.h
  include/ioj/sim/collision/collision_uniform_grid.h
  include/ioj/sim/deterministic_bias.h
  include/ioj/sim/direct_damage_events.h
  include/ioj/sim/entities/direct_damage_events.h
  include/ioj/sim/entities/entity_death_info.h
  include/ioj/sim/entities/registry_handle_state.h
  include/ioj/sim/entities/team_list.h
  include/ioj/sim/entity_aabbs.h
  include/ioj/sim/entity_cell_data_operations.h
  include/ioj/sim/entity_cell_data.h
  include/ioj/sim/entity_death_info.h
  include/ioj/sim/entity_handle.h
  include/ioj/sim/entity_history.h
  include/ioj/sim/entity_life_state.h
  include/ioj/sim/entity_overlaps.h
  include/ioj/sim/entity_registry_bookkeeping.h
  include/ioj/sim/entity_registry_query.h
  include/ioj/sim/entity_registry_refresh.h
  include/ioj/sim/entity_registry_statistics.h
  include/ioj/sim/entity_registry_view.h
  include/ioj/sim/entity_registry.h
  include/ioj/sim/entity_telemetry.h
  include/ioj/sim/entity_types.h
  include/ioj/sim/entity_world_bounds.h
  include/ioj/sim/fighter_diagnostics.h
  include/ioj/sim/fighter_entity_data.h
  include/ioj/sim/fighter_frame_spawn_queue.h
  include/ioj/sim/fighter_navigation_scratch.h
  include/ioj/sim/fighter_navigation.h
  include/ioj/sim/fighter_order_queue.h
  include/ioj/sim/fighter_reassignment.h
  include/ioj/sim/fighter_spawn_queue.h
  include/ioj/sim/fighter_types.h
  include/ioj/sim/fighters/command_interface.h
  include/ioj/sim/fighters/frame_spawn_queue.h
  include/ioj/sim/fighters/order.h
  include/ioj/sim/fighters/phase_interface.h
  include/ioj/sim/fighters/sim.h
  include/ioj/sim/fighters/spawn_queue.h
  include/ioj/sim/fixed_tick_loop.h
  include/ioj/sim/fixed_tick.h
  include/ioj/sim/frame_collision_scratch.h
  include/ioj/sim/frame_direct_damage_events.h
  include/ioj/sim/frame_hit_details.h
  include/ioj/sim/frame_laser_spawn_requests.h
  include/ioj/sim/frame_rotators3f.h
  include/ioj/sim/frame_trace_hits.h
  include/ioj/sim/frame_vectors3f.h
  include/ioj/sim/index_span.h
  include/ioj/sim/laser_frame_output.h
  include/ioj/sim/laser_hit_details.h
  include/ioj/sim/laser_soa.h
  include/ioj/sim/laser_source.h
  include/ioj/sim/lasers/frame_scratch.h
  include/ioj/sim/lasers/phase_interface.h
  include/ioj/sim/lasers/sim.h
  include/ioj/sim/level_event_types.h
  include/ioj/sim/level_read_view.h
  include/ioj/sim/level_sim.h
  include/ioj/sim/reference_level_simulation_data.h
  include/ioj/sim/level_telemetry_block_history_types.h
  include/ioj/sim/level_telemetry_current_state.h
  include/ioj/sim/level_telemetry_history_stats.h
  include/ioj/sim/level_telemetry_manager.h
  include/ioj/sim/level_telemetry_run_data.h
  include/ioj/sim/level_telemetry_snapshot.h
  include/ioj/sim/levels/compiled_level_events.h
  include/ioj/sim/levels/level_compilation.h
  include/ioj/sim/levels/level_definition.h
  include/ioj/sim/levels/level_event_group_counts.h
  include/ioj/sim/levels/level_event_manager.h
  include/ioj/sim/levels/level_event_schedule.h
  include/ioj/sim/levels/level_initialisation_data.h
  include/ioj/sim/levels/level_mission_event_type.h
  include/ioj/sim/levels/level_mission_events.h
  include/ioj/sim/levels/level_mission_initialisation_data.h
  include/ioj/sim/levels/level_mission_mode.h
  include/ioj/sim/levels/level_runtime_events.h
  include/ioj/sim/levels/level_spawn_manager.h
  include/ioj/sim/line_traces.h
  include/ioj/sim/memory/game_memory_backing_lease.h
  include/ioj/sim/memory/game_memory_backing.h
  include/ioj/sim/memory/game_memory_block.h
  include/ioj/sim/memory/game_memory_config.h
  include/ioj/sim/memory/game_memory.h
  include/ioj/sim/mission_manager.h
  include/ioj/sim/missions/mission_fail_reason.h
  include/ioj/sim/missions/mission_mode.h
  include/ioj/sim/missions/mission_state.h
  include/ioj/sim/navigation_telemetry.h
  include/ioj/sim/overlap_handler.h
  include/ioj/sim/player/control_mode.h
  include/ioj/sim/player/fire_rate.h
  include/ioj/sim/player/flight_mode.h
  include/ioj/sim/player/laser_firing_state.h
  include/ioj/sim/player/phase_interface.h
  include/ioj/sim/player/player_read_view.h
  include/ioj/sim/player/ship_laser_mode.h
  include/ioj/sim/player/sim.h
  include/ioj/sim/player/space_ship_common.h
  include/ioj/sim/profiling.h
  include/ioj/sim/query_thread_buffer_pool.h
  include/ioj/sim/query_thread_buffers.h
  include/ioj/sim/registry_entity_data.h
  include/ioj/sim/registry_entity_handles.h
  include/ioj/sim/rotator_math.h
  include/ioj/sim/rotator_types.h
  include/ioj/sim/rotator3d.h
  include/ioj/sim/rotators3f.h
  include/ioj/sim/ship_flight_model.h
  include/ioj/sim/ship_health.h
  include/ioj/sim/sim_clock.h
  include/ioj/sim/sim_config.h
  include/ioj/sim/sim_state.h
  include/ioj/sim/sim_tick.h
  include/ioj/sim/sim_time.h
  include/ioj/sim/base_sim_config.h
  include/ioj/sim/spatial_query_manager.h
  include/ioj/sim/spatial_query_telemetry.h
  include/ioj/sim/spatial.h
  include/ioj/sim/spawned_entity_handles.h
  include/ioj/sim/spinner_entity_data.h
  include/ioj/sim/spinners/phase_interface.h
  include/ioj/sim/spinners/sim.h
  include/ioj/sim/step_response.h
  include/ioj/sim/system_read_views.h
  include/ioj/sim/telemetry/level_telemetry_block_history.h
  include/ioj/sim/telemetry/level_telemetry_history.h
  include/ioj/sim/telemetry/level_telemetry_run_end_reason.h
  include/ioj/sim/telemetry/level_telemetry_run_record.h
  include/ioj/sim/trace_hits.h
  include/ioj/sim/transform3d.h
  include/ioj/sim/turret_entity_data.h
  include/ioj/sim/turret_spawn_data.h
  include/ioj/sim/turrets/phase_interface.h
  include/ioj/sim/turrets/sim.h
  include/ioj/sim/turrets/spawn_data.h
  include/ioj/sim/vector_operations.h
  include/ioj/sim/vector_types.h
  include/ioj/sim/vectors3f.h
  include/ioj/sim/world_aabb_operations.h
  include/ioj/sim/world_aabbs.h
  src/attack_distance_band.cpp
  src/batch_operations.cpp
  src/capital_ships/phase_interface.cpp
  src/capital_ships/sim.cpp
  src/collision_events.cpp
  src/collision_grid_entity_storage.cpp
  src/collision_grid_static_storage.cpp
  src/collision_grid.cpp
  src/collision_overlap_storage.cpp
  src/collision/collision_system.cpp
  src/collision/collision_uniform_grid.cpp
  src/deterministic_bias.cpp
  src/entity_registry_bookkeeping.cpp
  src/entity_registry_query.cpp
  src/entity_registry_refresh.cpp
  src/entity_registry_statistics.cpp
  src/entity_registry.cpp
  src/entity_world_bounds.cpp
  src/fighter_frame_spawn_queue.cpp
  src/fighter_navigation.cpp
  src/fighters/phase_interface.cpp
  src/fighters/sim.cpp
  src/fixed_tick.cpp
  src/frame_collision_scratch.cpp
  src/frame_direct_damage_events.cpp
  src/frame_hit_details.cpp
  src/frame_laser_spawn_requests.cpp
  src/frame_rotators3f.cpp
  src/frame_trace_hits.cpp
  src/frame_vectors3f.cpp
  src/laser_frame_output.cpp
  src/lasers/phase_interface.cpp
  src/lasers/sim.cpp
  src/level_event_types.cpp
  src/level_sim.cpp
  src/reference_level_simulation_data.cpp
  src/level_telemetry_manager.cpp
  src/levels/level_compilation.cpp
  src/levels/level_definition.cpp
  src/levels/level_event_manager.cpp
  src/levels/level_event_schedule.cpp
  src/levels/level_spawn_manager.cpp
  src/memory/game_memory.cpp
  src/mission_manager.cpp
  src/overlap_handler.cpp
  src/player/phase_interface.cpp
  src/player/sim.cpp
  src/query_thread_buffer_pool.cpp
  src/query_thread_buffers.cpp
  src/rotator_math.cpp
  src/rotator3d.cpp
  src/ship_flight_model.cpp
  src/ship_health.cpp
  src/sim_time.cpp
  src/spatial_query_manager.cpp
  src/spatial_query_telemetry.cpp
  src/spatial.cpp
  src/spinners/phase_interface.cpp
  src/spinners/sim.cpp
  src/step_response.cpp
  src/telemetry/level_telemetry_block_history.cpp
  src/transform3d.cpp
  src/turrets/phase_interface.cpp
  src/turrets/sim.cpp
)
target_compile_features(native-simulation PUBLIC cxx_std_23)
target_include_directories(native-simulation PUBLIC
  include
  "${PROJECT_SOURCE_DIR}/native/core/include"
  "${PROJECT_SOURCE_DIR}/native/lispb/native_soa/include"
)
target_link_libraries(native-simulation
  PUBLIC sandbox::handmade_math native-memory native-core
  PRIVATE sandbox::warnings_as_errors
)
if(SANDBOX_WITH_TRACY)
  target_compile_definitions(native-simulation PRIVATE SANDBOX_WITH_TRACY)
  target_link_libraries(native-simulation PRIVATE Tracy::TracyClient)
endif()

set_target_properties(native-simulation PROPERTIES
  OUTPUT_NAME SandboxNativeSimulation
)
if(SANDBOX_WITH_UNREAL)
  target_link_libraries(native-simulation PRIVATE sandbox::unreal_avx2)
  set_target_properties(native-simulation PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY
      "${PROJECT_SOURCE_DIR}/Binaries/Native/Simulation/${SANDBOX_NATIVE_TOOLCHAIN}/${UE_PLATFORM}/${UE_CONFIGURATION}"
  )
endif()

add_subdirectory(tests)

foreach(native_simulation_target IN ITEMS native-simulation native-simulation-tests)
  set_property(TARGET ${native_simulation_target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_compile_definitions(${native_simulation_target} PRIVATE _ITERATOR_DEBUG_LEVEL=0)

  if(SANDBOX_MSVC_FRONTEND)
    target_compile_options(${native_simulation_target} PRIVATE /permissive-)
    if(UE_CONFIGURATION MATCHES "^(Development|Shipping|Test)$")
      target_compile_options(${native_simulation_target} PRIVATE /O2)
    endif()
  else()
    if(UE_CONFIGURATION MATCHES "^(Development|Shipping|Test)$")
      target_compile_options(${native_simulation_target} PRIVATE -O2)
    endif()
  endif()

  if(UE_CONFIGURATION MATCHES "^(Shipping|Test)$")
    target_compile_definitions(${native_simulation_target} PRIVATE NDEBUG)
  endif()
endforeach()
