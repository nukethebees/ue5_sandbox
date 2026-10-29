set(IOJ_HOST_TOOLS_CONFIGURATION Release CACHE STRING "Build configuration for the Lispb host generator")
set_property(CACHE IOJ_HOST_TOOLS_CONFIGURATION PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
option(IOJ_ENABLE_BUILD_PROFILING "Enable compiler build timing for participating targets" OFF)

set(lispb_host_cache "")
foreach(variable IN ITEMS
    CMAKE_TOOLCHAIN_FILE
    CMAKE_C_COMPILER
    CMAKE_CXX_COMPILER
    CMAKE_MAKE_PROGRAM
    CMAKE_C_COMPILER_LAUNCHER
    CMAKE_CXX_COMPILER_LAUNCHER
    IOJ_ENABLE_BUILD_PROFILING
)
  string(APPEND lispb_host_cache "set(${variable} [==[${${variable}}]==] CACHE STRING \"\" FORCE)\n")
endforeach()
if(DEFINED CMAKE_COMPILE_WARNING_AS_ERROR)
  set(lispb_host_warnings_as_errors "${CMAKE_COMPILE_WARNING_AS_ERROR}")
else()
  set(lispb_host_warnings_as_errors ON)
endif()
string(APPEND lispb_host_cache
  "set(CMAKE_COMPILE_WARNING_AS_ERROR ${lispb_host_warnings_as_errors} CACHE BOOL \"\" FORCE)\n"
  "set(CMAKE_BUILD_TYPE ${IOJ_HOST_TOOLS_CONFIGURATION} CACHE STRING \"\" FORCE)\n"
  "set(IOJ_LISPB_HOST_BUILD ON CACHE INTERNAL \"\" FORCE)\n"
  "set(IOJ_WITH_UNREAL OFF CACHE BOOL \"\" FORCE)\n"
  "set(IOJ_BUILD_DEVELOPER_TOOLS OFF CACHE BOOL \"\" FORCE)\n"
  "set(IOJ_ENABLE_ASAN OFF CACHE BOOL \"\" FORCE)\n"
  "set(IOJ_ENABLE_CLANG_TIDY OFF CACHE BOOL \"\" FORCE)\n"
  "set(IOJ_ENABLE_PROFILING OFF CACHE BOOL \"\" FORCE)\n"
  "set(CMAKE_UNITY_BUILD OFF CACHE BOOL \"\" FORCE)\n"
)

# Share only compatible host builds; game configuration and unity do not affect the generator.
set(lispb_host_identity "${lispb_host_cache}")
foreach(variable IN ITEMS
    CMAKE_GENERATOR CMAKE_GENERATOR_PLATFORM CMAKE_GENERATOR_TOOLSET
    CMAKE_C_COMPILER_VERSION CMAKE_CXX_COMPILER_VERSION CMAKE_SYSTEM_PROCESSOR
    LLVM_ROOT msvc_toolchain_directory msvc_sdk_directory
    windows_ToolchainVersion windows_WindowsSDKVersion
)
  string(APPEND lispb_host_identity "${variable}=${${variable}}\n")
endforeach()
foreach(compiler IN ITEMS "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}")
  file(SIZE "${compiler}" compiler_size)
  file(TIMESTAMP "${compiler}" compiler_time UTC)
  string(APPEND lispb_host_identity "${compiler_size};${compiler_time}\n")
endforeach()
string(SHA256 lispb_host_key "${lispb_host_identity}")
string(SUBSTRING "${lispb_host_key}" 0 16 lispb_host_key)
set(lispb_host_build "${PROJECT_SOURCE_DIR}/out/host-tools/lispb/${lispb_host_key}/${IOJ_HOST_TOOLS_CONFIGURATION}")
set(lispb_host_executable "${lispb_host_build}/bin/lispb${CMAKE_EXECUTABLE_SUFFIX}")
set(lispb_host_driver "${CMAKE_CURRENT_BINARY_DIR}/build_lispb_host.cmake")
configure_file("${CMAKE_CURRENT_LIST_DIR}/build_lispb_host.cmake.in" "${lispb_host_driver}" @ONLY)

add_custom_target(lispb-host
  COMMAND "${CMAKE_COMMAND}" -P "${lispb_host_driver}"
  BYPRODUCTS "${lispb_host_executable}"
  COMMENT "Checking shared Lispb host build"
  USES_TERMINAL
  VERBATIM
)

add_executable(lispb-executable IMPORTED GLOBAL)
set_target_properties(lispb-executable PROPERTIES IMPORTED_LOCATION "${lispb_host_executable}")
add_dependencies(lispb-executable lispb-host)
message(STATUS "Lispb host: ${lispb_host_build}")
