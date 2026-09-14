if(NOT WIN32)
  return()
endif()

add_library(jobserver-client)
add_library(jobserver::client ALIAS jobserver-client)
target_sources(jobserver-client PRIVATE
  lib/src/client.cpp
  lib/src/protocol.cpp
  lib/src/transport.cpp
  lib/src/types.cpp
)
target_include_directories(jobserver-client PUBLIC lib/include)
target_link_libraries(jobserver-client
  PUBLIC sandbox::warnings_as_errors
  PRIVATE nlohmann_json::nlohmann_json advapi32
)
target_compile_definitions(jobserver-client PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)

add_library(jobserver-scheduler)
target_sources(jobserver-scheduler PRIVATE
  daemon/scheduler.cpp
)
target_include_directories(jobserver-scheduler PUBLIC
  daemon
  lib/include
)
target_link_libraries(jobserver-scheduler PUBLIC
  sandbox::warnings_as_errors
  nlohmann_json::nlohmann_json
)

add_library(jobserver-supervisor)
target_sources(jobserver-supervisor PRIVATE daemon/supervisor.cpp)
target_include_directories(jobserver-supervisor PUBLIC
  daemon
  lib/include
)
target_link_libraries(jobserver-supervisor PUBLIC sandbox::warnings_as_errors)
target_compile_definitions(jobserver-supervisor PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)

add_executable(jobserver)
target_sources(jobserver PRIVATE cli/main.cpp)
target_link_libraries(jobserver PRIVATE
  jobserver::client
  nlohmann_json::nlohmann_json
)

add_executable(jobserverd)
target_sources(jobserverd PRIVATE
  daemon/main.cpp
  daemon/server.cpp
)
target_link_libraries(jobserverd PRIVATE
  jobserver-scheduler
  jobserver-supervisor
  jobserver::client
  advapi32
)
target_compile_definitions(jobserverd PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)

add_executable(jobserver-test-helper)
target_sources(jobserver-test-helper PRIVATE test_helpers/main.cpp)
target_link_libraries(jobserver-test-helper PRIVATE sandbox::warnings_as_errors)

add_executable(jobserver-test-client)
target_sources(jobserver-test-client PRIVATE test_helpers/client_main.cpp)
target_link_libraries(jobserver-test-client PRIVATE jobserver::client)

add_executable(jobserver-tests)
target_sources(jobserver-tests PRIVATE
  tests/protocol_tests.cpp
  tests/scheduler_tests.cpp
  tests/server_tests.cpp
  tests/supervisor_tests.cpp
)
target_link_libraries(jobserver-tests PRIVATE
  jobserver-scheduler
  jobserver-supervisor
  jobserver::client
  GTest::gtest_main
)
target_compile_definitions(jobserver-tests PRIVATE
  JOBSERVER_DAEMON_PATH="$<TARGET_FILE:jobserverd>"
  JOBSERVER_TEST_CLIENT_PATH="$<TARGET_FILE:jobserver-test-client>"
  JOBSERVER_TEST_HELPER_PATH="$<TARGET_FILE:jobserver-test-helper>"
)
add_dependencies(jobserver-tests
  jobserver-test-client
  jobserver-test-helper
  jobserverd
)
add_test(NAME jobserver-tests COMMAND jobserver-tests)
set_tests_properties(jobserver-tests PROPERTIES LABELS "all;jobserver")

install(TARGETS jobserver jobserverd
  RUNTIME DESTINATION bin
  COMPONENT jobserver
)

add_custom_target(install-jobserver
  COMMAND pwsh -NoProfile -File
    "${CMAKE_CURRENT_SOURCE_DIR}/install/Stop-Jobserver.ps1"
    -ClientPath "${SANDBOX_JOBSERVER_INSTALL_ROOT}/bin/jobserver.exe"
  COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
    --component jobserver
    --prefix "${SANDBOX_JOBSERVER_INSTALL_ROOT}"
  COMMAND pwsh -NoProfile -File
    "${CMAKE_CURRENT_SOURCE_DIR}/install/Register-Jobserver.ps1"
    -DaemonPath "${SANDBOX_JOBSERVER_INSTALL_ROOT}/bin/jobserverd.exe"
  DEPENDS jobserver jobserverd
  COMMENT "Installing and registering the canonical per-user jobserver"
  USES_TERMINAL
  VERBATIM
)
