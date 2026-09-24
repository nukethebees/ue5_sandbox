project(CpuFeatures VERSION 0.11.0 LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
    FORCE)
endif()

option(BUILD_SHARED_LIBS "Build library as shared." OFF)
if(BUILD_SHARED_LIBS AND UNIX)
  option(CMAKE_POSITION_INDEPENDENT_CODE "Build with Position Independent Code." ON)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cpu_features.cmake")
sandbox_add_cpu_features("${IOJ_CPU_FEATURES_SOURCE_DIRECTORY}")
