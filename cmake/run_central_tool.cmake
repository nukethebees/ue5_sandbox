find_program(tool NAMES "${ToolName}" PATHS ENV PATH NO_DEFAULT_PATH NO_CACHE)
if(NOT tool)
  message(FATAL_ERROR
    "${ToolName} is missing from PATH. Ask the maintainer to install it with "
    "agent-task install-central-tools and add its per-tool bin directory to PATH.")
endif()

set(arguments "")
set(collect_arguments FALSE)
math(EXPR last_argument "${CMAKE_ARGC} - 1")
foreach(index RANGE 0 ${last_argument})
  if(collect_arguments)
    string(REPLACE ";" "\\;" argument "${CMAKE_ARGV${index}}")
    list(APPEND arguments "${argument}")
  elseif(CMAKE_ARGV${index} STREQUAL "--")
    set(collect_arguments TRUE)
  endif()
endforeach()
execute_process(COMMAND "${tool}" ${arguments} COMMAND_ERROR_IS_FATAL ANY)
