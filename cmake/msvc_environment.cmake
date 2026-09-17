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

function(sandbox_make_msvc_library_linker_flags output_variable)
  set(linker_flags)
  foreach(library_directory IN LISTS ARGN)
    string(REPLACE "\\" "/" library_directory "${library_directory}")
    string(APPEND linker_flags " /LIBPATH:\"${library_directory}\"")
  endforeach()
  set(${output_variable} "${linker_flags}" PARENT_SCOPE)
endfunction()
