include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/jobserver_commands.cmake")

if(NOT DEFINED ENV{LOCALAPPDATA} OR "$ENV{LOCALAPPDATA}" STREQUAL "")
  message(FATAL_ERROR "LOCALAPPDATA is required to locate the per-user jobserver.")
endif()

cmake_path(SET IOJ_JOBSERVER_INSTALL_ROOT NORMALIZE
  "$ENV{LOCALAPPDATA}/NukeTheBees/jobserver")
find_program(IOJ_JOBSERVER_PATH NAMES jobserver PATHS ENV PATH NO_DEFAULT_PATH NO_CACHE)
if(IOJ_JOBSERVER_PATH)
  set(IOJ_JOBSERVER_CLI "${IOJ_JOBSERVER_PATH}")
else()
  set(IOJ_JOBSERVER_CLI jobserver.exe)
endif()

function(sandbox_jobserver_command output_variable mode kind operation)
  sandbox_make_jobserver_command(command "${IOJ_JOBSERVER_CLI}"
    "${CMAKE_SOURCE_DIR}" "${mode}" "${kind}" "${operation}")
  set(${output_variable} "${command}" PARENT_SCOPE)
endfunction()

function(sandbox_configure_jobserver)
  if(IOJ_JOBSERVER_PATH)
    foreach(language IN ITEMS C CXX)
      sandbox_jobserver_command(compile_launcher STANDARD build "Compile ${language}")
      sandbox_jobserver_command(link_launcher STANDARD build "Link ${language}")
      set(CMAKE_${language}_COMPILER_LAUNCHER
        ${compile_launcher} ${CMAKE_${language}_COMPILER_LAUNCHER} PARENT_SCOPE)
      set(CMAKE_${language}_LINKER_LAUNCHER
        ${link_launcher} ${CMAKE_${language}_LINKER_LAUNCHER} PARENT_SCOPE)
    endforeach()
    # CMake applies this only when add_test's command names an executable target.
    sandbox_jobserver_command(test_launcher STANDARD test "Native executable test")
    set(CMAKE_TEST_LAUNCHER ${test_launcher} ${CMAKE_TEST_LAUNCHER} PARENT_SCOPE)
  else()
    message(STATUS
      "The per-user jobserver is not installed; bootstrap targets will build "
      "without cross-worktree scheduling. Run the install-jobserver target.")
  endif()
endfunction()

function(sandbox_add_jobserver_status_target)
  add_custom_target(jobserver-status
    COMMAND "${IOJ_JOBSERVER_CLI}" status
    COMMENT "Showing per-user jobserver state"
    VERBATIM
  )
endfunction()
