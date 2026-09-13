add_executable(native-simulation-tests)
target_sources(native-simulation-tests PRIVATE
  test_level_simulation.cpp
  test_entity_registry_invariants.cpp
  test_overlap_handler.cpp
  test_attack_distance_band.cpp
  test_entity_aabb_overlaps.cpp
  test_fighter_live_cap.cpp
  test_level_telemetry_block_history.cpp
  test_game_memory.cpp
  test_moved_entities_battle.cpp
  test_spinner_spawning.cpp
  fighter_entity_data_test.cpp
  scenarios/test_capital_command_fighters.cpp
  scenarios/test_capital_fighter_handles.cpp
  scenarios/test_collision_uniform_grid.cpp
  scenarios/test_entity_registry.cpp
  scenarios/test_fighter_attack.cpp
  scenarios/test_fighter_los_failure.cpp
  scenarios/test_fighters_intercept_capital.cpp
  scenarios/test_fighters_standby_transition.cpp
  scenarios/test_laser_lifecycle.cpp
  scenarios/test_mission_manager.cpp
  scenarios/test_player_ship_vs_capital.cpp
  scenarios/test_simulation_core_regressions.cpp
  scenarios/test_spatial_query_empty.cpp
  scenarios/test_spatial_query_manager.cpp
  scenarios/test_turret_acquisition_regressions.cpp
  scenarios/test_turrets_kill_one.cpp
  worldless_simulation_tests.cpp
  support/worldless_simulation_test.cpp
  support/simulation_fixture.cpp
  deterministic_bias_tests.cpp
  entity_handle_tests.cpp
  fixed_tick_tests.cpp
  index_span_tests.cpp
  ship_health_tests.cpp
  spatial_tests.cpp
  step_response_tests.cpp
)
target_compile_features(native-simulation-tests PRIVATE cxx_std_23)
target_link_libraries(native-simulation-tests PRIVATE
  native-simulation
  sandbox::warnings_as_errors
)

if(SANDBOX_WITH_TRACY AND SANDBOX_WITH_UNREAL)
  target_sources(native-simulation-tests PRIVATE tracy_lifetime.cpp)
  target_link_libraries(native-simulation-tests PRIVATE Tracy::TracyClient)
  add_custom_command(TARGET native-simulation-tests POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "$<TARGET_FILE:TracyClient>"
      "$<TARGET_FILE_DIR:native-simulation-tests>"
    VERBATIM
  )
endif()

target_link_libraries(native-simulation-tests PRIVATE GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(native-simulation-tests PROPERTIES LABELS "unit;all;native-simulation")
