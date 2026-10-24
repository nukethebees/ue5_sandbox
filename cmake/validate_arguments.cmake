include_guard(GLOBAL)

function(ioj_validate_arguments prefix caller)
  if(${prefix}_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "${caller} received unexpected arguments: ${${prefix}_UNPARSED_ARGUMENTS}")
  endif()

  foreach(keyword IN LISTS ARGN)
    if(NOT ${prefix}_${keyword})
      message(FATAL_ERROR "${caller} requires ${keyword}.")
    endif()
  endforeach()
endfunction()
