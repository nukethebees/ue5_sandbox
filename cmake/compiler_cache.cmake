foreach(language IN ITEMS C CXX)
  if(NOT CMAKE_${language}_COMPILER_LAUNCHER)
    continue()
  endif()

  list(GET CMAKE_${language}_COMPILER_LAUNCHER 0 compiler_launcher)
  get_filename_component(compiler_launcher_name "${compiler_launcher}" NAME_WE)
  if(compiler_launcher_name STREQUAL "ccache" AND
     CMAKE_${language}_COMPILER_ID MATCHES "Clang" AND
     CMAKE_${language}_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    # ccache cannot parse CMake's -clang: depfile flags.
    set(CMAKE_DEPFILE_FLAGS_${language} "/showIncludes")
    set(CMAKE_${language}_DEPFILE_FORMAT msvc)
    list(APPEND CMAKE_${language}_COMPILE_OPTIONS_CREATE_PCH
      -Xclang -fno-pch-timestamp
    )
  endif()
endforeach()
