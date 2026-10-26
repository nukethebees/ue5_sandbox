include(ExternalProject)

set(IOJ_HOST_TOOLS_CONFIGURATION Release CACHE STRING "Build configuration for the Lispb host generator")
set_property(CACHE IOJ_HOST_TOOLS_CONFIGURATION PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

set(lispb_host_build "${CMAKE_BINARY_DIR}/host-tools/lispb")
set(lispb_host_executable "${lispb_host_build}/bin/lispb${CMAKE_EXECUTABLE_SUFFIX}")
set(lispb_host_cache_args)
foreach(variable IN ITEMS
    CMAKE_TOOLCHAIN_FILE
    CMAKE_C_COMPILER
    CMAKE_CXX_COMPILER
    CMAKE_MAKE_PROGRAM
    CMAKE_C_COMPILER_LAUNCHER
    CMAKE_CXX_COMPILER_LAUNCHER
    CMAKE_COMPILE_WARNING_AS_ERROR
    IOJ_ENABLE_BUILD_PROFILING
)
  if(DEFINED ${variable})
    string(REPLACE ";" "|" value "${${variable}}")
    list(APPEND lispb_host_cache_args "-D${variable}:STRING=${value}")
  endif()
endforeach()

ExternalProject_Add(lispb-host
  SOURCE_DIR "${PROJECT_SOURCE_DIR}"
  BINARY_DIR "${lispb_host_build}"
  DOWNLOAD_COMMAND ""
  UPDATE_COMMAND ""
  LIST_SEPARATOR "|"
  CMAKE_CACHE_ARGS
    ${lispb_host_cache_args}
    "-DCMAKE_BUILD_TYPE:STRING=${IOJ_HOST_TOOLS_CONFIGURATION}"
    -DIOJ_LISPB_HOST_BUILD:INTERNAL=ON
    -DIOJ_WITH_UNREAL:BOOL=OFF
    -DIOJ_BUILD_DEVELOPER_TOOLS:BOOL=OFF
    -DIOJ_ENABLE_ASAN:BOOL=OFF
    -DIOJ_ENABLE_CLANG_TIDY:BOOL=OFF
    -DIOJ_ENABLE_PROFILING:BOOL=OFF
  CONFIGURE_ENVIRONMENT_MODIFICATION "LLVM_ROOT=set:${LLVM_ROOT}"
  BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR>
    --config "${IOJ_HOST_TOOLS_CONFIGURATION}" --target lispb
  BUILD_ALWAYS TRUE
  BUILD_BYPRODUCTS "${lispb_host_executable}"
  INSTALL_COMMAND ""
  EXCLUDE_FROM_ALL TRUE
)

add_executable(lispb-executable IMPORTED GLOBAL)
set_target_properties(lispb-executable PROPERTIES IMPORTED_LOCATION "${lispb_host_executable}")
add_dependencies(lispb-executable lispb-host)
