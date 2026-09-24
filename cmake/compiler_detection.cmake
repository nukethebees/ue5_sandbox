include_guard(GLOBAL)

function(ioj_is_msvc_frontend outvar)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR
     CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(${outvar} TRUE)
  else()
    set(${outvar} FALSE)
  endif()

  return(PROPAGATE ${outvar})
endfunction()

function(ioj_is_clang_cl outvar)
  ioj_is_msvc_frontend(_is_msvc_frontend)

  set(${outvar} FALSE)
  if(_is_msvc_frontend AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(${outvar} TRUE)
  endif()

  return(PROPAGATE ${outvar})
endfunction()