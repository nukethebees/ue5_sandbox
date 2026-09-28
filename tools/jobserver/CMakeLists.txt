if(NOT WIN32)
  return()
endif()

add_library(jobserver-client
  lib/src/authority.cpp lib/src/client.cpp lib/src/executor.cpp
  lib/src/protocol.cpp lib/src/transport.cpp lib/src/types.cpp)
add_library(jobserver::client ALIAS jobserver-client)
target_include_directories(jobserver-client PUBLIC lib/include)
target_link_libraries(jobserver-client PUBLIC ioj::cxx_standard ioj::cxx_conformance
  nlohmann_json::nlohmann_json PRIVATE advapi32)

add_library(jobserver-gate daemon/gate.cpp daemon/journal.cpp)
target_include_directories(jobserver-gate PUBLIC daemon lib/include)
target_link_libraries(jobserver-gate PUBLIC ioj::cxx_standard ioj::cxx_conformance nlohmann_json::nlohmann_json)

add_executable(jobserver cli/main.cpp cli/broker.cpp cli/doctor.cpp)
target_link_libraries(jobserver PRIVATE jobserver::client ole32 oleaut32 taskschd)
add_executable(jobserverd WIN32 daemon/main.cpp daemon/server.cpp)
target_link_libraries(jobserverd PRIVATE jobserver-gate jobserver::client advapi32)

add_executable(jobserver-gate-tests tests/gate_tests.cpp tests/journal_tests.cpp)
target_link_libraries(jobserver-gate-tests PRIVATE jobserver-gate GTest::gtest_main)
add_test(NAME jobserver-gate-tests COMMAND jobserver-gate-tests)
set_tests_properties(jobserver-gate-tests PROPERTIES LABELS "jobserver")

add_executable(jobserver-test-helper test_helpers/main.cpp)
target_link_libraries(jobserver-test-helper PRIVATE ioj::cxx_standard ioj::cxx_conformance)
add_executable(jobserver-tests tests/protocol_tests.cpp tests/server_tests.cpp)
target_link_libraries(jobserver-tests PRIVATE jobserver::client GTest::gtest_main)
target_compile_definitions(jobserver-tests PRIVATE
  JOBSERVER_CLI_PATH="$<TARGET_FILE:jobserver>"
  JOBSERVER_DAEMON_PATH="$<TARGET_FILE:jobserverd>"
  JOBSERVER_TEST_HELPER_PATH="$<TARGET_FILE:jobserver-test-helper>")
add_dependencies(jobserver-tests jobserver jobserverd jobserver-test-helper)
add_test(NAME jobserver-tests COMMAND jobserver-tests)
set_tests_properties(jobserver-tests PROPERTIES LABELS "jobserver;integration;subprocess" TIMEOUT 120)

foreach(target IN ITEMS jobserver-client jobserver-gate jobserver jobserverd jobserver-gate-tests jobserver-test-helper jobserver-tests)
  ioj_enable_warnings(${target})
  target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endforeach()
add_test(NAME jobserver-version COMMAND jobserver --version)
set_tests_properties(jobserver-version PROPERTIES LABELS "jobserver" PASS_REGULAR_EXPRESSION "jobserver ")

option(IOJ_JOBSERVER_SYSTEM_TESTS "Register tests that mutate the canonical installation" OFF)
if(IOJ_JOBSERVER_SYSTEM_TESTS)
  add_test(NAME jobserver-system-upgrade
    COMMAND pwsh -NoProfile -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/system/UpgradeLifecycle.ps1"
      -BuildDirectory "${CMAKE_BINARY_DIR}" -ClientPath "${IOJ_JOBSERVER_CLI}"
      -BuiltClientPath "$<TARGET_FILE:jobserver>" -BuiltDaemonPath "$<TARGET_FILE:jobserverd>")
  set_tests_properties(jobserver-system-upgrade PROPERTIES LABELS "jobserver;jobserver-system"
    RUN_SERIAL TRUE TIMEOUT 300)
endif()
install(TARGETS jobserver jobserverd
  RUNTIME DESTINATION bin
  COMPONENT jobserver
)

add_custom_target(install-jobserver
  COMMAND pwsh -NoProfile -File
    "${CMAKE_CURRENT_SOURCE_DIR}/install/Install-Jobserver.ps1"
    -BuiltClientPath "$<TARGET_FILE:jobserver>"
    -BuiltDaemonPath "$<TARGET_FILE:jobserverd>"
    -InstallRoot "${IOJ_JOBSERVER_INSTALL_ROOT}"
    -RegisterScript "${CMAKE_CURRENT_SOURCE_DIR}/install/Register-Jobserver.ps1"
    -AsanEnabled "$<BOOL:${IOJ_ENABLE_ASAN}>"
  DEPENDS jobserver jobserverd
  COMMENT "Installing and registering the canonical per-user jobserver"
  USES_TERMINAL
  VERBATIM
)
