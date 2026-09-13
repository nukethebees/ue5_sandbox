find_package(GTest CONFIG REQUIRED)

add_library(native-simulation STATIC)
target_sources(native-simulation PRIVATE
  include/sandbox/simulation/attack_distance_band.h
  include/sandbox/simulation/batch_damage.h
  include/sandbox/simulation/capital_death_event.h
  include/sandbox/simulation/capital_fighter_orders.h
  include/sandbox/simulation/capital_fighter_reassignment.h
  include/sandbox/simulation/capital_ship_queries.h
  include/sandbox/simulation/collision_events.h
  include/sandbox/simulation/collision_grid_entity_storage.h
  include/sandbox/simulation/collision_grid_overlap_query.h
  include/sandbox/simulation/collision_grid_queries.h
  include/sandbox/simulation/collision_grid_static_storage.h
  include/sandbox/simulation/collision_overlap_storage.h
  include/sandbox/simulation/direct_damage_events.h
  include/sandbox/simulation/entity_aabbs.h
  include/sandbox/simulation/entity_cell_data.h
  include/sandbox/simulation/entity_cell_data_operations.h
  include/sandbox/simulation/entity_combat_accounting.h
  include/sandbox/simulation/entity_death_accounting.h
  include/sandbox/simulation/entity_death_info.h
  include/sandbox/simulation/entity_history.h
  include/sandbox/simulation/entity_overlap_operations.h
  include/sandbox/simulation/entity_overlaps.h
  include/sandbox/simulation/entity_registry_bookkeeping.h
  include/sandbox/simulation/entity_registry_history.h
  include/sandbox/simulation/entity_registry_query.h
  include/sandbox/simulation/entity_registry_refresh.h
  include/sandbox/simulation/entity_registry_spawn.h
  include/sandbox/simulation/entity_registry_statistics.h
  include/sandbox/simulation/entity_registry_update.h
  include/sandbox/simulation/entity_telemetry.h
  include/sandbox/simulation/entity_world_bounds.h
  include/sandbox/simulation/fighter_damage_response.h
  include/sandbox/simulation/fighter_attack_preparation.h
  include/sandbox/simulation/fighter_aiming.h
  include/sandbox/simulation/fighter_firing.h
  include/sandbox/simulation/fighter_firing_position.h
  include/sandbox/simulation/fighter_firing_scratch.h
  include/sandbox/simulation/fighter_movement.h
  include/sandbox/simulation/fighter_navigation.h
  include/sandbox/simulation/fighter_navigation_state.h
  include/sandbox/simulation/fighter_orders.h
  include/sandbox/simulation/fighter_navigation_application.h
  include/sandbox/simulation/fighter_navigation_schedule.h
  include/sandbox/simulation/fighter_navigation_scans.h
  include/sandbox/simulation/fighter_navigation_scratch.h
  include/sandbox/simulation/fighter_order_queue.h
  include/sandbox/simulation/fighter_reassignment.h
  include/sandbox/simulation/fighter_spawn_admission.h
  include/sandbox/simulation/fighter_spawn_initialization.h
  include/sandbox/simulation/rotator_math.h
  include/sandbox/simulation/fighter_spawn_queue.h
  include/sandbox/simulation/fighter_frame_spawn_queue.h
  include/sandbox/simulation/fighter_task_layout.h
  include/sandbox/simulation/fighter_targeting.h
  include/sandbox/simulation/frame_collision_scratch.h
  include/sandbox/simulation/frame_direct_damage_events.h
  include/sandbox/simulation/frame_hit_details.h
  include/sandbox/simulation/frame_laser_spawn_requests.h
  include/sandbox/simulation/frame_rotators3f.h
  include/sandbox/simulation/frame_trace_hits.h
  include/sandbox/simulation/frame_vectors3f.h
  include/sandbox/simulation/laser_collision_response.h
  include/sandbox/simulation/laser_source.h
  include/sandbox/simulation/laser_spawn_initialization.h
  include/sandbox/simulation/level_event_types.h
  include/sandbox/simulation/level_telemetry_block_history_types.h
  include/sandbox/simulation/level_telemetry_current_state.h
  include/sandbox/simulation/level_telemetry_history_stats.h
  include/sandbox/simulation/level_telemetry_run_data.h
  include/sandbox/simulation/level_telemetry_snapshot.h
  include/sandbox/simulation/line_traces.h
  include/sandbox/simulation/navigation_telemetry.h
  include/sandbox/simulation/query_thread_buffer_pool.h
  include/sandbox/simulation/player_thrust.h
  include/sandbox/simulation/query_thread_buffers.h
  include/sandbox/simulation/registry_entity_handles.h
  include/sandbox/simulation/rotator_types.h
  include/sandbox/simulation/rotators3f.h
  include/sandbox/simulation/ship_health.h
  include/sandbox/simulation/ship_flight_model.h
  include/sandbox/simulation/simulation_config.h
  include/sandbox/simulation/spatial_query_telemetry.h
  include/sandbox/simulation/spawned_entity_handles.h
  include/sandbox/simulation/telemetry_statistics.h
  include/sandbox/simulation/trace_hits.h
  include/sandbox/simulation/turret_targeting.h
  include/sandbox/simulation/vector_types.h
  include/sandbox/simulation/vectors3f.h
  include/sandbox/simulation/world_aabbs.h
  include/sandbox/simulation/world_aabb_operations.h
  src/attack_distance_band.cpp
  src/batch_damage.cpp
  src/capital_fighter_orders.cpp
  src/capital_fighter_reassignment.cpp
  src/capital_ship_queries.cpp
  src/collision_events.cpp
  src/collision_grid.cpp
  src/collision_grid_entity_storage.cpp
  src/collision_grid_overlap_query.cpp
  src/collision_grid_queries.cpp
  src/collision_grid_static_storage.cpp
  src/collision_overlap_storage.cpp
  src/deterministic_bias.cpp
  src/entity_combat_accounting.cpp
  src/entity_death_accounting.cpp
  src/entity_overlap_operations.cpp
  src/entity_registry_bookkeeping.cpp
  src/entity_registry_history.cpp
  src/entity_registry_query.cpp
  src/entity_registry_refresh.cpp
  src/entity_registry_spawn.cpp
  src/entity_registry_statistics.cpp
  src/entity_registry_update.cpp
  src/entity_world_bounds.cpp
  src/fixed_tick.cpp
  src/fighter_damage_response.cpp
  src/fighter_attack_preparation.cpp
  src/fighter_aiming.cpp
  src/fighter_firing.cpp
  src/fighter_firing_position.cpp
  src/fighter_firing_scratch.cpp
  src/fighter_movement.cpp
  src/fighter_navigation.cpp
  src/fighter_navigation_state.cpp
  src/fighter_orders.cpp
  src/fighter_spawn_admission.cpp
  src/fighter_spawn_initialization.cpp
  src/rotator_math.cpp
  src/fighter_frame_spawn_queue.cpp
  src/fighter_task_layout.cpp
  src/fighter_targeting.cpp
  src/fighter_navigation_application.cpp
  src/fighter_navigation_schedule.cpp
  src/fighter_navigation_scans.cpp
  src/fighter_navigation_scratch.cpp
  src/frame_collision_scratch.cpp
  src/frame_direct_damage_events.cpp
  src/frame_hit_details.cpp
  src/frame_laser_spawn_requests.cpp
  src/frame_rotators3f.cpp
  src/frame_trace_hits.cpp
  src/frame_vectors3f.cpp
  src/laser_collision_response.cpp
  src/laser_spawn_initialization.cpp
  src/level_event_types.cpp
  src/query_thread_buffer_pool.cpp
  src/query_thread_buffers.cpp
  src/simulation_clock.cpp
  src/ship_health.cpp
  src/player_thrust.cpp
  src/ship_flight_model.cpp
  src/spatial.cpp
  src/spatial_query_telemetry.cpp
  src/step_response.cpp
  src/telemetry_statistics.cpp
  src/turret_targeting.cpp
)
target_compile_features(native-simulation PUBLIC cxx_std_23)
target_include_directories(native-simulation PUBLIC
  include
  "${PROJECT_SOURCE_DIR}/native/core/include"
  "${PROJECT_SOURCE_DIR}/native/lispb/native_soa/include"
)
target_link_libraries(native-simulation
  PUBLIC sandbox::handmade_math
  PRIVATE sandbox::warnings_as_errors
)

set(NATIVE_SIMULATION_LIBRARY_DIRECTORY
  "${PROJECT_SOURCE_DIR}/Binaries/Native/Simulation/${UE_PLATFORM}/${UE_CONFIGURATION}"
)
set_target_properties(native-simulation PROPERTIES
  OUTPUT_NAME SandboxNativeSimulation
  ARCHIVE_OUTPUT_DIRECTORY "${NATIVE_SIMULATION_LIBRARY_DIRECTORY}"
)

add_executable(native-simulation-tests)
target_sources(native-simulation-tests PRIVATE
  tests/deterministic_bias_tests.cpp
  tests/entity_handle_tests.cpp
  tests/fixed_tick_tests.cpp
  tests/index_span_tests.cpp
  tests/ship_health_tests.cpp
  tests/spatial_tests.cpp
  tests/step_response_tests.cpp
)
target_compile_features(native-simulation-tests PRIVATE cxx_std_23)
target_link_libraries(native-simulation-tests PRIVATE
  native-simulation
  sandbox::warnings_as_errors
)

if(WIN32)
  get_target_property(NATIVE_SIMULATION_GTEST_INCLUDE_DIRECTORIES
    GTest::gtest INTERFACE_INCLUDE_DIRECTORIES)
  get_target_property(NATIVE_SIMULATION_GTEST_LIBRARY GTest::gtest IMPORTED_IMPLIB_RELEASE)
  get_target_property(NATIVE_SIMULATION_GTEST_MAIN_LIBRARY GTest::gtest_main IMPORTED_IMPLIB_RELEASE)
  get_target_property(NATIVE_SIMULATION_GTEST_DLL GTest::gtest IMPORTED_LOCATION_RELEASE)
  get_target_property(NATIVE_SIMULATION_GTEST_MAIN_DLL GTest::gtest_main IMPORTED_LOCATION_RELEASE)

  foreach(required_property IN ITEMS
      NATIVE_SIMULATION_GTEST_INCLUDE_DIRECTORIES
      NATIVE_SIMULATION_GTEST_LIBRARY
      NATIVE_SIMULATION_GTEST_MAIN_LIBRARY
      NATIVE_SIMULATION_GTEST_DLL
      NATIVE_SIMULATION_GTEST_MAIN_DLL)
    if(NOT DEFINED ${required_property} OR "${${required_property}}" MATCHES "-NOTFOUND$")
      message(FATAL_ERROR "GTest does not provide ${required_property} for the release CRT.")
    endif()
  endforeach()

  target_include_directories(native-simulation-tests SYSTEM PRIVATE
    ${NATIVE_SIMULATION_GTEST_INCLUDE_DIRECTORIES})
  target_link_libraries(native-simulation-tests PRIVATE
    "${NATIVE_SIMULATION_GTEST_MAIN_LIBRARY}"
    "${NATIVE_SIMULATION_GTEST_LIBRARY}"
  )
  add_custom_command(TARGET native-simulation-tests POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${NATIVE_SIMULATION_GTEST_DLL}"
      "${NATIVE_SIMULATION_GTEST_MAIN_DLL}"
      "$<TARGET_FILE_DIR:native-simulation-tests>"
    VERBATIM
  )
else()
  target_link_libraries(native-simulation-tests PRIVATE GTest::gtest_main)
endif()

include(GoogleTest)
gtest_discover_tests(native-simulation-tests PROPERTIES LABELS "unit;all;native-simulation")

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
