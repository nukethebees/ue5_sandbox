find_package(GTest CONFIG REQUIRED)

add_library(native-simulation STATIC)
target_sources(native-simulation PRIVATE
  include/sandbox/simulation/direct_damage_events.h
  include/sandbox/simulation/entity_aabbs.h
  include/sandbox/simulation/entity_cell_data.h
  include/sandbox/simulation/entity_cell_data_operations.h
  include/sandbox/simulation/entity_death_info.h
  include/sandbox/simulation/entity_history.h
  include/sandbox/simulation/entity_overlaps.h
  include/sandbox/simulation/entity_world_bounds.h
  include/sandbox/simulation/fighter_order_queue.h
  include/sandbox/simulation/fighter_reassignment.h
  include/sandbox/simulation/frame_trace_hits.h
  include/sandbox/simulation/frame_vectors3f.h
  include/sandbox/simulation/laser_source.h
  include/sandbox/simulation/level_telemetry_snapshot.h
  include/sandbox/simulation/line_traces.h
  include/sandbox/simulation/registry_entity_handles.h
  include/sandbox/simulation/rotator_types.h
  include/sandbox/simulation/rotators3f.h
  include/sandbox/simulation/simulation_config.h
  include/sandbox/simulation/trace_hits.h
  include/sandbox/simulation/vector_types.h
  include/sandbox/simulation/vectors3f.h
  include/sandbox/simulation/world_aabbs.h
  include/sandbox/simulation/world_aabb_operations.h
  src/collision_grid.cpp
  src/deterministic_bias.cpp
  src/entity_world_bounds.cpp
  src/fixed_tick.cpp
  src/frame_trace_hits.cpp
  src/frame_vectors3f.cpp
  src/simulation_clock.cpp
  src/spatial.cpp
  src/step_response.cpp
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
