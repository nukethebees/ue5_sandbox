include_guard(GLOBAL)

function(sandbox_make_jobserver_command output_variable cli worktree mode kind operation)
  set(known_kinds
    build
    unreal-build
    test
    unreal-test
    unreal-command
    benchmark
    static-analysis
    format
    generate
    package
    command
  )
  list(FIND known_kinds "${kind}" kind_index)
  if(kind_index EQUAL -1)
    message(FATAL_ERROR "Unknown jobserver kind '${kind}'.")
  endif()

  if(mode STREQUAL "BENCHMARK")
    set(machine_claim --exclusive machine --exclusive benchmark)
  elseif(mode STREQUAL "STANDARD")
    set(machine_claim --shared machine)
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
