if(NOT CMAKE_HOST_WIN32)
  message(FATAL_ERROR "The clang-cl toolchain supports only Windows hosts.")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/windows_environment.cmake")

find_program(IOJ_CLANG_CL_EXECUTABLE NAMES clang-cl REQUIRED)

# Keep clang's automatic toolchain discovery pinned when building from a plain shell.
set(windows_driver_arguments
  "/vctoolsdir \"${msvc_toolchain_directory}\" /winsdkdir \"${msvc_sdk_directory}\" /winsdkversion ${windows_WindowsSDKVersion}")
set(CMAKE_C_FLAGS_INIT "${windows_driver_arguments}")
set(CMAKE_CXX_FLAGS_INIT "${windows_driver_arguments}")

set(CMAKE_C_COMPILER "${IOJ_CLANG_CL_EXECUTABLE}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${IOJ_CLANG_CL_EXECUTABLE}" CACHE FILEPATH "" FORCE)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL CACHE STRING "" FORCE)
set(IOJ_NATIVE_TOOLCHAIN clang-cl CACHE STRING "Native compiler toolchain" FORCE)
