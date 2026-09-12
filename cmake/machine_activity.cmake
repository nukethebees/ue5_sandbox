include_guard(GLOBAL)

# Every operation receives a monotonically increasing ticket while state.lock is
# held. Standard work waits only for older benchmarks. A benchmark waits for all
# older work, while every newer request waits behind that benchmark. Each request
# file is also an OS-backed lease, so process exit makes stale requests reclaimable.
set(SANDBOX_MACHINE_ACTIVITY_PROTOCOL_VERSION 1)

function(_sandbox_activity_root output_variable)
  if(NOT DEFINED ENV{TEMP} OR "$ENV{TEMP}" STREQUAL "")
    message(FATAL_ERROR
      "TEMP is required for the machine-wide Sandbox activity gate.")
  endif()

  cmake_path(SET activity_root NORMALIZE
    "$ENV{TEMP}/SandboxUnrealBuild/activity/v1")
  set(${output_variable} "${activity_root}" PARENT_SCOPE)
endfunction()

function(_sandbox_activity_prepare root)
  file(MAKE_DIRECTORY "${root}/requests")
endfunction()

function(_sandbox_activity_parse_request request_path
    ticket_output mode_output request_id_output)
  get_filename_component(request_name "${request_path}" NAME)
  if(request_name MATCHES
      "^0*([1-9][0-9]*)-(standard|benchmark)-([0-9a-f]+)[.]lock$")
    set(ticket "${CMAKE_MATCH_1}")
    set(mode "${CMAKE_MATCH_2}")
    set(request_id "${CMAKE_MATCH_3}")
    string(LENGTH "${request_id}" request_id_length)
    if(request_id_length EQUAL 32)
      set(${ticket_output} "${ticket}" PARENT_SCOPE)
      set(${mode_output} "${mode}" PARENT_SCOPE)
      set(${request_id_output} "${request_id}" PARENT_SCOPE)
      return()
    endif()
  endif()

  set(${ticket_output} "" PARENT_SCOPE)
  set(${mode_output} "" PARENT_SCOPE)
  set(${request_id_output} "" PARENT_SCOPE)
endfunction()

function(_sandbox_activity_request_is_live request_path output_variable)
  if(NOT EXISTS "${request_path}")
    set(${output_variable} FALSE PARENT_SCOPE)
    return()
  endif()

  file(LOCK "${request_path}" GUARD FUNCTION TIMEOUT 0
    RESULT_VARIABLE lock_result)
  if(lock_result STREQUAL "0")
    file(LOCK "${request_path}" RELEASE)
    set(${output_variable} FALSE PARENT_SCOPE)
  else()
    set(${output_variable} TRUE PARENT_SCOPE)
  endif()
endfunction()

function(_sandbox_activity_clean_requests root output_variable)
  set(live_requests)
  file(GLOB requests LIST_DIRECTORIES FALSE "${root}/requests/*.lock")
  foreach(request_path IN LISTS requests)
    _sandbox_activity_parse_request(
      "${request_path}" ticket mode request_id)
    if(NOT ticket)
      # Unknown request names fail closed so damaged or newer state stays visible.
      list(APPEND live_requests "${request_path}")
      continue()
    endif()

    _sandbox_activity_request_is_live("${request_path}" request_is_live)
    if(request_is_live)
      list(APPEND live_requests "${request_path}")
    else()
      file(REMOVE "${request_path}")
    endif()
  endforeach()

  set(${output_variable} "${live_requests}" PARENT_SCOPE)
endfunction()

function(_sandbox_activity_pad_ticket ticket output_variable)
  set(padded "00000000000000000000${ticket}")
  string(LENGTH "${padded}" padded_length)
  math(EXPR start "${padded_length} - 20")
  string(SUBSTRING "${padded}" ${start} 20 padded)
  set(${output_variable} "${padded}" PARENT_SCOPE)
endfunction()

function(_sandbox_activity_allocate root mode operation request_id
    request_path_output ticket_output)
  file(LOCK "${root}/state.lock" GUARD FUNCTION
    RESULT_VARIABLE state_lock_result)
  if(NOT state_lock_result STREQUAL "0")
    message(FATAL_ERROR
      "Failed to acquire activity state lock '${root}/state.lock': "
      "${state_lock_result}")
  endif()

  _sandbox_activity_clean_requests("${root}" live_requests)
  set(next_ticket 1)
  foreach(request_path IN LISTS live_requests)
    _sandbox_activity_parse_request(
      "${request_path}" ticket existing_mode existing_request_id)
    if(ticket)
      math(EXPR candidate "${ticket} + 1")
      if(candidate GREATER next_ticket)
        set(next_ticket "${candidate}")
      endif()
    endif()
  endforeach()

  _sandbox_activity_pad_ticket("${next_ticket}" padded_ticket)
  set(request_path
    "${root}/requests/${padded_ticket}-${mode}-${request_id}.lock")
  string(TIMESTAMP requested_at "%Y-%m-%dT%H:%M:%S.%fZ" UTC)
  string(REPLACE "\n" " " operation "${operation}")
  string(REPLACE "\r" " " operation "${operation}")
  string(CONCAT metadata
    "protocol=${SANDBOX_MACHINE_ACTIVITY_PROTOCOL_VERSION}\n"
    "ticket=${next_ticket}\n"
    "mode=${mode}\n"
    "request=${request_id}\n"
    "requested_utc=${requested_at}\n"
    "operation=${operation}\n"
    "source=${CMAKE_CURRENT_SOURCE_DIR}\n"
  )
  file(WRITE "${request_path}" "${metadata}")
  file(LOCK "${request_path}" GUARD PROCESS TIMEOUT 0
    RESULT_VARIABLE request_lock_result)
  if(NOT request_lock_result STREQUAL "0")
    file(REMOVE "${request_path}")
    message(FATAL_ERROR
      "Failed to acquire activity request '${request_path}': "
      "${request_lock_result}")
  endif()

  set(${request_path_output} "${request_path}" PARENT_SCOPE)
  set(${ticket_output} "${next_ticket}" PARENT_SCOPE)
endfunction()

function(_sandbox_activity_get_blockers root mode ticket output_variable)
  file(LOCK "${root}/state.lock" GUARD FUNCTION
    RESULT_VARIABLE state_lock_result)
  if(NOT state_lock_result STREQUAL "0")
    message(FATAL_ERROR
      "Failed to acquire activity state lock '${root}/state.lock': "
      "${state_lock_result}")
  endif()

  _sandbox_activity_clean_requests("${root}" live_requests)
  set(blockers)
  foreach(request_path IN LISTS live_requests)
    _sandbox_activity_parse_request(
      "${request_path}" other_ticket other_mode other_request_id)
    if(NOT other_ticket)
      list(APPEND blockers "${request_path}")
    elseif(other_ticket LESS ticket AND
        (mode STREQUAL "benchmark" OR other_mode STREQUAL "benchmark"))
      list(APPEND blockers "${request_path}")
    endif()
  endforeach()

  set(${output_variable} "${blockers}" PARENT_SCOPE)
endfunction()

function(_sandbox_activity_wait_message mode operation wait_count blocker_count)
  math(EXPR message_remainder "${wait_count} % 100")
  if(wait_count EQUAL 1 OR message_remainder EQUAL 0)
    message(STATUS
      "Waiting for ${mode} machine activity access (${operation}): "
      "${blocker_count} older request(s)")
  endif()
endfunction()

function(sandbox_machine_activity_acquire mode operation)
  string(TOLOWER "${mode}" mode)
  if(NOT mode STREQUAL "standard" AND NOT mode STREQUAL "benchmark")
    message(FATAL_ERROR
      "Activity mode must be STANDARD or BENCHMARK, got '${mode}'.")
  endif()

  _sandbox_activity_root(root)
  _sandbox_activity_prepare("${root}")

  if(DEFINED ENV{SANDBOX_MACHINE_ACTIVITY_MODE} AND
      NOT "$ENV{SANDBOX_MACHINE_ACTIVITY_MODE}" STREQUAL "")
    set(parent_mode "$ENV{SANDBOX_MACHINE_ACTIVITY_MODE}")
    set(parent_root "$ENV{SANDBOX_MACHINE_ACTIVITY_ROOT}")
    set(parent_request "$ENV{SANDBOX_MACHINE_ACTIVITY_REQUEST}")
    if(NOT parent_root STREQUAL root)
      message(FATAL_ERROR
        "Nested activity token refers to '${parent_root}', expected '${root}'.")
    endif()
    _sandbox_activity_request_is_live("${parent_request}" parent_request_is_live)
    if(NOT parent_request_is_live)
      message(FATAL_ERROR
        "Nested activity token no longer has a live request: '${parent_request}'.")
    endif()
    _sandbox_activity_parse_request(
      "${parent_request}" parent_ticket marker_mode parent_request_id)
    if(NOT parent_ticket OR NOT marker_mode STREQUAL parent_mode)
      message(FATAL_ERROR "Nested activity token does not match its request file.")
    endif()

    if(parent_mode STREQUAL "benchmark" OR
        (parent_mode STREQUAL "standard" AND mode STREQUAL "standard"))
      set_property(GLOBAL PROPERTY SANDBOX_ACTIVITY_OWNED FALSE)
      return()
    endif()
    if(parent_mode STREQUAL "standard" AND mode STREQUAL "benchmark")
      message(FATAL_ERROR
        "Cannot upgrade nested standard activity access to benchmark access. "
        "Acquire benchmark access in the outermost command instead.")
    endif()
    message(FATAL_ERROR "Unrecognised nested activity mode '${parent_mode}'.")
  endif()

  string(RANDOM LENGTH 32 ALPHABET 0123456789abcdef request_id)
  _sandbox_activity_allocate(
    "${root}" "${mode}" "${operation}" "${request_id}" request_path ticket)

  set(wait_count 0)
  while(TRUE)
    _sandbox_activity_get_blockers("${root}" "${mode}" "${ticket}" blockers)
    if(NOT blockers)
      break()
    endif()

    list(LENGTH blockers blocker_count)
    math(EXPR wait_count "${wait_count} + 1")
    _sandbox_activity_wait_message(
      "${mode}" "${operation}" "${wait_count}" "${blocker_count}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.1)
  endwhile()

  set(ENV{SANDBOX_MACHINE_ACTIVITY_MODE} "${mode}")
  set(ENV{SANDBOX_MACHINE_ACTIVITY_ROOT} "${root}")
  set(ENV{SANDBOX_MACHINE_ACTIVITY_REQUEST} "${request_path}")
  set_property(GLOBAL PROPERTY SANDBOX_ACTIVITY_OWNED TRUE)
  set_property(GLOBAL PROPERTY SANDBOX_ACTIVITY_ROOT "${root}")
  set_property(GLOBAL PROPERTY SANDBOX_ACTIVITY_REQUEST "${request_path}")
  if(NOT MACHINE_ACTIVITY_QUIET)
    message(STATUS
      "Acquired ${mode} machine activity access (ticket ${ticket}): ${operation}")
  endif()
endfunction()

function(sandbox_machine_activity_release)
  get_property(owned GLOBAL PROPERTY SANDBOX_ACTIVITY_OWNED)
  if(NOT owned)
    return()
  endif()

  get_property(root GLOBAL PROPERTY SANDBOX_ACTIVITY_ROOT)
  get_property(request_path GLOBAL PROPERTY SANDBOX_ACTIVITY_REQUEST)
  file(LOCK "${root}/state.lock" GUARD FUNCTION
    RESULT_VARIABLE state_lock_result)
  if(NOT state_lock_result STREQUAL "0")
    message(FATAL_ERROR
      "Failed to acquire activity state lock '${root}/state.lock': "
      "${state_lock_result}")
  endif()
  file(LOCK "${request_path}" RELEASE)
  file(REMOVE "${request_path}")

  unset(ENV{SANDBOX_MACHINE_ACTIVITY_MODE})
  unset(ENV{SANDBOX_MACHINE_ACTIVITY_ROOT})
  unset(ENV{SANDBOX_MACHINE_ACTIVITY_REQUEST})
  set_property(GLOBAL PROPERTY SANDBOX_ACTIVITY_OWNED FALSE)
endfunction()

function(sandbox_machine_activity_status)
  _sandbox_activity_root(root)
  _sandbox_activity_prepare("${root}")
  file(LOCK "${root}/state.lock" GUARD FUNCTION
    RESULT_VARIABLE state_lock_result)
  if(NOT state_lock_result STREQUAL "0")
    message(FATAL_ERROR
      "Failed to acquire activity state lock '${root}/state.lock': "
      "${state_lock_result}")
  endif()
  _sandbox_activity_clean_requests("${root}" live_requests)
  file(LOCK "${root}/state.lock" RELEASE)

  message(STATUS "Machine activity root: ${root}")
  list(LENGTH live_requests request_count)
  message(STATUS "requests: ${request_count}")
  foreach(request_path IN LISTS live_requests)
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E cat "${request_path}"
      OUTPUT_VARIABLE metadata
      ERROR_QUIET
      RESULT_VARIABLE read_result
    )
    if(read_result EQUAL 0)
      string(STRIP "${metadata}" metadata)
      string(REPLACE "\n" ", " metadata "${metadata}")
      message(STATUS "  ${request_path}: ${metadata}")
    else()
      message(STATUS "  ${request_path}: retired while reading status")
    endif()
  endforeach()
endfunction()
