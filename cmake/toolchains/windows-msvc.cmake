if(NOT CMAKE_HOST_WIN32)
  message(FATAL_ERROR "The MSVC toolchain supports only Windows hosts.")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/windows_environment.cmake")

find_program(IOJ_NINJA_EXECUTABLE NAMES ninja REQUIRED)
set(CMAKE_MAKE_PROGRAM "${IOJ_NINJA_EXECUTABLE}" CACHE FILEPATH "" FORCE)

set(CMAKE_C_COMPILER "${msvc_cl}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${msvc_cl}" CACHE FILEPATH "" FORCE)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL CACHE STRING "" FORCE)
set(IOJ_NATIVE_TOOLCHAIN msvc CACHE STRING "Native compiler toolchain" FORCE)
