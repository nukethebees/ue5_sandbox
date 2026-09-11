cmake_minimum_required(VERSION 4.4.2)

foreach(required_variable
    UNREAL_LOCK_PATH
    UNREAL_ENGINE_ROOT
    UNREAL_WORKING_DIRECTORY)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${required_variable} is required.")
  endif()
endforeach()

set(unreal_command)
set(found_command_separator FALSE)
math(EXPR last_argument_index "${CMAKE_ARGC} - 1")

foreach(argument_index RANGE 0 ${last_argument_index})
  set(argument_variable "CMAKE_ARGV${argument_index}")
  set(argument "${${argument_variable}}")

  if(found_command_separator)
    list(APPEND unreal_command "${argument}")
  elseif(argument STREQUAL "--")
    set(found_command_separator TRUE)
  endif()
endforeach()

if(NOT found_command_separator OR NOT unreal_command)
  message(FATAL_ERROR "An Unreal command must be supplied after '--'.")
endif()

list(JOIN unreal_command " " unreal_command_display)
message(STATUS
  "Waiting for Unreal build lock for '${UNREAL_ENGINE_ROOT}': ${unreal_command_display}")

file(LOCK "${UNREAL_LOCK_PATH}"
  GUARD PROCESS
  RESULT_VARIABLE lock_result
)
if(NOT lock_result STREQUAL "0")
  message(FATAL_ERROR
    "Failed to acquire Unreal build lock '${UNREAL_LOCK_PATH}': ${lock_result}")
endif()

message(STATUS "Acquired Unreal build lock '${UNREAL_LOCK_PATH}'.")

execute_process(
  COMMAND ${unreal_command}
  WORKING_DIRECTORY "${UNREAL_WORKING_DIRECTORY}"
  RESULT_VARIABLE command_result
)

if(NOT command_result MATCHES "^[0-9]+$")
  message(FATAL_ERROR "Failed to run Unreal command: ${command_result}")
endif()

if(NOT command_result EQUAL 0)
  message(STATUS "Unreal command exited with code ${command_result}.")
  cmake_language(EXIT "${command_result}")
endif()

message(STATUS "Unreal command completed successfully.")
