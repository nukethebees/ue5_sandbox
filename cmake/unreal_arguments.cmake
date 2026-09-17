include_guard(GLOBAL)

function(sandbox_make_automation_filter_expression output_variable)
  set(filters ${ARGN})
  list(TRANSFORM filters PREPEND "StartsWith:")
  list(JOIN filters "+" expression)
  set(${output_variable} "${expression}" PARENT_SCOPE)
endfunction()

function(sandbox_make_space_game_test_arguments output_variable time_scale)
  set(arguments)
  if(time_scale)
    if(NOT time_scale MATCHES "^[0-9]+([.][0-9]+)?$" OR time_scale LESS_EQUAL 0)
      message(FATAL_ERROR "SANDBOX_SPACE_GAME_TEST_TIME_SCALE must be empty or a positive number.")
    endif()
    list(APPEND arguments "-SpaceGameTestTimeScale=${time_scale}")
  endif()
  set(${output_variable} "${arguments}" PARENT_SCOPE)
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
