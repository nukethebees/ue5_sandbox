foreach(language IN ITEMS C CXX)
  if(CMAKE_${language}_COMPILER_ID MATCHES "Clang|MSVC")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
      "${CMAKE_${language}_COMPILER}"
    )
    if(NOT CMAKE_${language}_COMPILER STREQUAL pch_compiler)
      set(pch_compiler "${CMAKE_${language}_COMPILER}")
      file(SHA256 "${pch_compiler}" pch_compiler_hash)
    endif()
    # Replacing the compiler at the same path must invalidate its serialized PCH format.
    list(APPEND CMAKE_${language}_COMPILE_OPTIONS_CREATE_PCH
      "-DIOJ_PCH_COMPILER_SHA256=${pch_compiler_hash}"
    )
    if(CMAKE_${language}_COMPILER_ID MATCHES "Clang" AND
       CMAKE_${language}_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" AND DEFINED msvc_cl)
      set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${msvc_cl}"
      )
      if(NOT pch_msvc_compiler STREQUAL msvc_cl)
        set(pch_msvc_compiler "${msvc_cl}")
        file(SHA256 "${pch_msvc_compiler}" pch_msvc_compiler_hash)
      endif()
      # clang-cl derives its MSVC compatibility version from this executable.
      list(APPEND CMAKE_${language}_COMPILE_OPTIONS_CREATE_PCH
        "-DIOJ_PCH_MSVC_SHA256=${pch_msvc_compiler_hash}"
      )
    endif()
  endif()

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
