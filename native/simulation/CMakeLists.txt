find_package(GTest CONFIG REQUIRED)

add_library(native-simulation STATIC)
target_sources(native-simulation PRIVATE
  src/deterministic_bias.cpp
  src/fixed_tick.cpp
  src/simulation_clock.cpp
  src/spatial.cpp
  src/step_response.cpp
)
target_compile_features(native-simulation PUBLIC cxx_std_23)
target_include_directories(native-simulation PUBLIC include)
target_link_libraries(native-simulation PRIVATE sandbox::warnings_as_errors)

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
