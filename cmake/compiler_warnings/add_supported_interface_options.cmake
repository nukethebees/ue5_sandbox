function(target_add_supported_options interface_target)
  cmake_parse_arguments(PARSE_ARGV 1 supported_options "" "SOURCE" "COMPILE;LINK")

  if(supported_options_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "target_add_supported_options(${interface_target}) received unexpected arguments: "
      "${supported_options_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT TARGET "${interface_target}")
    message(FATAL_ERROR
      "target_add_supported_options(${interface_target}) requires an existing target.")
  endif()

  get_target_property(interface_target_type "${interface_target}" TYPE)
  if(NOT interface_target_type STREQUAL "INTERFACE_LIBRARY")
    message(FATAL_ERROR
      "target_add_supported_options(${interface_target}) requires an interface library.")
  endif()

  if(NOT supported_options_COMPILE AND NOT supported_options_LINK)
    message(FATAL_ERROR
      "target_add_supported_options(${interface_target}) requires COMPILE or LINK options.")
  endif()

  if(NOT DEFINED supported_options_SOURCE)
    set(supported_options_SOURCE "int main() { return 0; }")
  endif()

  string(CONCAT options_identity
    "${CMAKE_CXX_COMPILER_ID};${CMAKE_CXX_COMPILER_VERSION};"
    "${CMAKE_CXX_COMPILER_FRONTEND_VARIANT};${supported_options_SOURCE};"
    "${supported_options_COMPILE};${supported_options_LINK}"
  )
  string(SHA256 options_hash "${options_identity}")
  set(support_variable "SANDBOX_SUPPORTED_OPTIONS_${options_hash}")

  if(NOT DEFINED ${support_variable})
    set(try_compile_arguments)
    if(supported_options_COMPILE)
      list(APPEND try_compile_arguments
        COMPILE_DEFINITIONS ${supported_options_COMPILE})
    endif()
    if(supported_options_LINK)
      list(APPEND try_compile_arguments
        LINK_OPTIONS ${supported_options_LINK})
    endif()

    try_compile(options_compile_succeeded
      SOURCE_FROM_VAR supported_options.cpp supported_options_SOURCE
      ${try_compile_arguments}
      CXX_STANDARD 23
      CXX_STANDARD_REQUIRED TRUE
      CXX_EXTENSIONS TRUE
      OUTPUT_VARIABLE options_compile_output
      NO_CACHE
    )

    string(TOLOWER "${options_compile_output}" options_compile_output_lower)
    string(CONCAT unsupported_option_pattern
      "unknown (argument|warning option)|unrecognized command-line option|"
      "ignoring unknown option|command line warning d9002|"
      "argument unused during compilation"
    )

    if(options_compile_succeeded AND
       NOT options_compile_output_lower MATCHES "${unsupported_option_pattern}")
      set(options_supported TRUE)
    else()
      set(options_supported FALSE)
    endif()

    set(${support_variable} "${options_supported}" CACHE INTERNAL
      "Whether compiler and linker options for ${interface_target} are supported")
  endif()

  if(${support_variable})
    if(supported_options_COMPILE)
      target_compile_options(${interface_target} INTERFACE ${supported_options_COMPILE})
    endif()
    if(supported_options_LINK)
      target_link_options(${interface_target} INTERFACE ${supported_options_LINK})
    endif()
  endif()
endfunction()
