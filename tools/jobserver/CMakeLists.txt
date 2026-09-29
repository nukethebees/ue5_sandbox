if(NOT WIN32)
  return()
endif()

add_library(jobserver-client
  lib/src/client.cpp
  lib/src/protocol.cpp lib/src/transport.cpp lib/src/types.cpp)
add_library(jobserver::client ALIAS jobserver-client)
target_include_directories(jobserver-client PUBLIC lib/include)
target_link_libraries(jobserver-client PUBLIC ioj::cxx_standard ioj::cxx_conformance
  nlohmann_json::nlohmann_json PRIVATE advapi32)

add_library(jobserver-gate daemon/gate.cpp daemon/journal.cpp)
target_include_directories(jobserver-gate PUBLIC daemon lib/include)
target_link_libraries(jobserver-gate PUBLIC ioj::cxx_standard ioj::cxx_conformance nlohmann_json::nlohmann_json)

add_executable(jobserver cli/main.cpp cli/doctor.cpp)
target_link_libraries(jobserver PRIVATE jobserver::client ole32 oleaut32 taskschd)
add_executable(jobserverd WIN32 daemon/main.cpp daemon/server.cpp)
target_link_libraries(jobserverd PRIVATE jobserver-gate jobserver::client advapi32)

add_executable(jobserver-gate-tests tests/gate_tests.cpp)
target_link_libraries(jobserver-gate-tests PRIVATE jobserver-gate GTest::gtest_main)
add_test(NAME jobserver-gate-tests COMMAND jobserver-gate-tests)
set_tests_properties(jobserver-gate-tests PROPERTIES LABELS "jobserver")

add_executable(jobserver-tests tests/protocol_tests.cpp tests/server_tests.cpp daemon/server.cpp)
target_link_libraries(jobserver-tests PRIVATE jobserver::client jobserver-gate GTest::gtest_main)
add_test(NAME jobserver-tests COMMAND jobserver-tests)
set_tests_properties(jobserver-tests PROPERTIES LABELS "jobserver;integration" TIMEOUT 120)

foreach(target IN ITEMS jobserver-client jobserver-gate jobserver jobserverd jobserver-gate-tests jobserver-tests)
  ioj_enable_warnings(${target})
  target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endforeach()
add_test(NAME jobserver-version COMMAND jobserver --version)
set_tests_properties(jobserver-version PROPERTIES LABELS "jobserver" PASS_REGULAR_EXPRESSION "jobserver ")

install(TARGETS jobserver jobserverd
  RUNTIME DESTINATION bin
  COMPONENT jobserver
)

add_custom_target(install-jobserver
  COMMAND pwsh -NoProfile -File
    "${CMAKE_CURRENT_SOURCE_DIR}/install/Install-Jobserver.ps1"
    -BuiltClientPath "$<TARGET_FILE:jobserver>"
    -BuiltDaemonPath "$<TARGET_FILE:jobserverd>"
    -InstallRoot "$ENV{LOCALAPPDATA}/NukeTheBees/jobserver"
    -RegisterScript "${CMAKE_CURRENT_SOURCE_DIR}/install/Register-Jobserver.ps1"
    -AsanEnabled "$<BOOL:${IOJ_ENABLE_ASAN}>"
  DEPENDS jobserver jobserverd
  COMMENT "Installing and registering the canonical per-user jobserver"
  USES_TERMINAL
  VERBATIM
)

add_test(NAME jobserver-reject-run COMMAND jobserver run)
add_test(NAME jobserver-reject-broker COMMAND jobserver broker)
set_tests_properties(jobserver-reject-run jobserver-reject-broker PROPERTIES
  LABELS "jobserver" WILL_FAIL TRUE)
add_custom_target(jobserver-status COMMAND jobserver status
  COMMENT "Showing scheduler admission state" VERBATIM)
