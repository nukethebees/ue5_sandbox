include_guard(GLOBAL)

function(sandbox_make_jobserver_command output_variable cli worktree mode kind operation)
  if(mode STREQUAL "BENCHMARK")
    set(machine_claim --exclusive machine)
  elseif(mode STREQUAL "STANDARD")
    # The outer broker/run owns ordinary machine admission.
    set(${output_variable} "" PARENT_SCOPE)
    return()
  else()
    message(FATAL_ERROR "Unknown jobserver activity mode '${mode}'.")
  endif()

  set(command
    "${cli}" run
    --name "${operation}"
    --kind "${kind}"
    --worktree "${worktree}"
    ${machine_claim}
    --
  )
  set(${output_variable} "${command}" PARENT_SCOPE)
endfunction()

function(sandbox_make_unreal_jobserver_resource output_variable canonical_root)
  string(TOLOWER "${canonical_root}" identity)
  string(SHA256 resource_hash "${identity}")
  set(${output_variable} "unreal-build/${resource_hash}" PARENT_SCOPE)
endfunction()
