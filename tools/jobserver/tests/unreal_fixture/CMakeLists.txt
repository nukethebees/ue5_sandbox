cmake_minimum_required(VERSION 4.4.2)
project(UnrealJobserverFixture LANGUAGES NONE)
enable_testing()
find_program(POWERSHELL_EXECUTABLE pwsh REQUIRED)
include("${SOURCE_ROOT}/cmake/unreal_jobserver.cmake")
set(SANDBOX_JOBSERVER_CLI "${CLIENT}")
sandbox_configure_unreal_jobserver("${ENGINE_ROOT}")
sandbox_unreal_jobserver_command(reader_command STANDARD "fixture reader")
sandbox_unreal_jobserver_command(benchmark_command BENCHMARK "fixture benchmark")
file(WRITE "${CMAKE_BINARY_DIR}/resource.txt" "${UE_ENGINE_JOBSERVER_RESOURCE}")
file(WRITE "${CMAKE_BINARY_DIR}/claims.txt" "${UE_JOBSERVER_COMMAND_PREFIX}\n${reader_command}\n${benchmark_command}\n")

set(UE_BUILD_SCRIPT "${HELPER}")
set(UE_EDITOR_CMD_EXE "${HELPER}")
set(UE_PLATFORM "${READY}")
set(UE_CONFIGURATION test)
set(SANDBOX_UPROJECT ready-sleep)
set(SANDBOX_LOCAL_DDC_DIR "${CMAKE_BINARY_DIR}/ddc")
include("${SOURCE_ROOT}/cmake/unreal.cmake")
add_unreal_target(writer ready-sleep)
add_custom_target(editor)
add_unreal_editor_test(reader-test ARGUMENTS "${READY}" LABELS unit TIMEOUT 60)
add_unreal_commandlet_target(commandlet COMMANDLET fixture COMMENT fixture)
add_unreal_benchmark_commandlet_target(benchmark-commandlet fixture)
add_custom_target(reader
  COMMAND ${reader_command} "${HELPER}" ready-sleep "${READY}"
  VERBATIM)
add_custom_target(descendant-reader
  COMMAND ${reader_command} "${HELPER}" ready-tree "${READY}" "${READY}.child" exit-root
  VERBATIM)
sandbox_jobserver_command(standard_activity_command STANDARD "fixture native")
add_custom_target(native
  COMMAND ${standard_activity_command} "${HELPER}" ready-sleep "${READY}"
  VERBATIM)

set(UE_PLATFORM Win64)
set(UE_CONFIGURATION Development)
set(UE_RUN_UAT_SCRIPT "${HELPER}")
set(UE_DEVELOPMENT_EDITOR_CMD_EXE "${HELPER}")
set(unreal_standard_activity_command "${reader_command}")
set(SANDBOX_GAME_STAGE_DIRECTORY "${CMAKE_BINARY_DIR}/stage")
set(SANDBOX_GAME_STAGE_EXECUTABLE "${HELPER}")
include("${SOURCE_ROOT}/cmake/unreal_packaging.cmake")
add_unreal_packaging_targets()
