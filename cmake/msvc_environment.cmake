include_guard(GLOBAL)

function(sandbox_get_msvc_environment_value output_variable environment_text environment_name)
  string(REGEX MATCH
    "(^|\n)SANDBOX_MSVC_${environment_name}=([^\r\n]*)"
    environment_match "${environment_text}")
  if(NOT environment_match)
    message(FATAL_ERROR "Visual Studio setup did not provide ${environment_name}.")
  endif()
  string(STRIP "${CMAKE_MATCH_2}" environment_value)
  set(${output_variable} "${environment_value}" PARENT_SCOPE)
endfunction()
