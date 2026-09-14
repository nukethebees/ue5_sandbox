if(NOT WIN32)
  return()
endif()

add_library(jobserver-client)
add_library(jobserver::client ALIAS jobserver-client)
target_sources(jobserver-client PRIVATE
  lib/src/authority.cpp
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
target_sources(jobserver PRIVATE
  cli/doctor.cpp
  cli/main.cpp
)
target_link_libraries(jobserver PRIVATE
  jobserver::client
  nlohmann_json::nlohmann_json
  ole32
  oleaut32
  taskschd
)
target_compile_definitions(jobserver PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)

add_executable(jobserverd WIN32)
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
target_compile_definitions(jobserver-test-client PRIVATE
  JOBSERVER_TEST_HELPER_PATH="$<TARGET_FILE:jobserver-test-helper>"
)
add_dependencies(jobserver-test-client jobserver-test-helper)

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
  JOBSERVER_CLI_PATH="$<TARGET_FILE:jobserver>"
  JOBSERVER_DAEMON_PATH="$<TARGET_FILE:jobserverd>"
  JOBSERVER_TEST_CLIENT_PATH="$<TARGET_FILE:jobserver-test-client>"
  JOBSERVER_TEST_HELPER_PATH="$<TARGET_FILE:jobserver-test-helper>"
)
add_dependencies(jobserver-tests
  jobserver
  jobserver-test-client
  jobserver-test-helper
  jobserverd
)
add_test(NAME jobserver-tests COMMAND jobserver-tests)
set_tests_properties(jobserver-tests PROPERTIES LABELS "all;jobserver")

option(SANDBOX_JOBSERVER_SYSTEM_TESTS
  "Register tests that mutate the canonical per-user jobserver installation"
  OFF
)
if(SANDBOX_JOBSERVER_SYSTEM_TESTS)
  add_test(NAME jobserver-system-cross-worktree
    COMMAND pwsh -NoProfile -File
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/system/CrossWorktree.ps1"
      -SourceRoot "${PROJECT_SOURCE_DIR}"
      -ClientPath "${SANDBOX_JOBSERVER_CLI}"
  )
  add_test(NAME jobserver-system-upgrade
    COMMAND pwsh -NoProfile -File
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/system/UpgradeLifecycle.ps1"
      -BuildDirectory "${CMAKE_BINARY_DIR}"
      -ClientPath "${SANDBOX_JOBSERVER_CLI}"
      -BuiltClientPath "$<TARGET_FILE:jobserver>"
      -BuiltDaemonPath "$<TARGET_FILE:jobserverd>"
  )
  set_tests_properties(
    jobserver-system-cross-worktree
    jobserver-system-upgrade
    PROPERTIES
      LABELS "jobserver;jobserver-system"
      RUN_SERIAL TRUE
      TIMEOUT 300
  )
endif()

# Keep the repair path independent from the installed jobserver version.
set_target_properties(
  jobserver-client
  jobserver-scheduler
  jobserver-supervisor
  jobserver
  jobserverd
  jobserver-test-helper
  jobserver-test-client
  jobserver-tests
  PROPERTIES
    CXX_COMPILER_LAUNCHER ""
    CXX_LINKER_LAUNCHER ""
)

install(TARGETS jobserver jobserverd
  RUNTIME DESTINATION bin
  COMPONENT jobserver
)

add_custom_target(install-jobserver
  COMMAND pwsh -NoProfile -File
    "${CMAKE_CURRENT_SOURCE_DIR}/install/Install-Jobserver.ps1"
    -BuiltClientPath "$<TARGET_FILE:jobserver>"
    -BuiltDaemonPath "$<TARGET_FILE:jobserverd>"
    -InstallRoot "${SANDBOX_JOBSERVER_INSTALL_ROOT}"
    -RegisterScript "${CMAKE_CURRENT_SOURCE_DIR}/install/Register-Jobserver.ps1"
  DEPENDS jobserver jobserverd
  COMMENT "Installing and registering the canonical per-user jobserver"
  USES_TERMINAL
  VERBATIM
)
