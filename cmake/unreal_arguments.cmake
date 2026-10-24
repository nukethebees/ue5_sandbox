include_guard(GLOBAL)

function(sandbox_make_automation_filter_expression output_variable)
  set(filters ${ARGN})
  list(TRANSFORM filters PREPEND "StartsWith:")
  list(JOIN filters "+" expression)
  set(${output_variable} "${expression}" PARENT_SCOPE)
endfunction()

function(sandbox_make_automation_exec_commands output_variable filter_expression)
  # Keep Unreal's startup discovery delay so editor session discovery cannot reset an active run.
  set(${output_variable}
    "-ExecCmds=Automation RunTests ${filter_expression}; Quit"
    PARENT_SCOPE)
endfunction()

function(sandbox_make_unreal_cook_arguments output_variable editor cook_directory)
  set(arguments
    -cook
    -skipstage
    "-unrealexe=${editor}"
    -ddc=NoZenLocalFallback
    "-cookoutputdir=${cook_directory}"
  )
  set(${output_variable} "${arguments}" PARENT_SCOPE)
endfunction()

function(sandbox_make_unreal_container_arguments output_variable cook_root stage_root)
  set(arguments
    -pak
    -iostore
    -compressed
    -prereqs
    "-cookoutputdir=${cook_root}"
    "-stagingdirectory=${stage_root}"
  )
  set(${output_variable} "${arguments}" PARENT_SCOPE)
endfunction()
