if(NOT CMAKE_HOST_WIN32)
  message(FATAL_ERROR "The clang-cl toolchain supports only Windows hosts.")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/windows_environment.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/../llvm_tools.cmake")
ioj_find_llvm_tool(IOJ_CLANG_CL_EXECUTABLE clang-cl)

if(LLVM_ROOT)
  ioj_find_llvm_tool(IOJ_LLVM_LIB_EXECUTABLE llvm-lib)
  ioj_find_llvm_tool(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS clang-scan-deps)
  set(CMAKE_AR "${IOJ_LLVM_LIB_EXECUTABLE}" CACHE FILEPATH "" FORCE)
  # Do not pick up LLVM linker/manifest tools from another installation on PATH.
  set(CMAKE_LINKER "${msvc_toolchain_directory}/bin/Hostx64/x64/link.exe" CACHE FILEPATH "" FORCE)
  set(CMAKE_MT "${msvc_sdk_directory}/bin/${windows_WindowsSDKVersion}/x64/mt.exe" CACHE FILEPATH "" FORCE)
endif()

# Keep clang's automatic toolchain discovery pinned when building from a plain shell.
set(windows_driver_arguments
  "/vctoolsdir \"${msvc_toolchain_directory}\" /winsdkdir \"${msvc_sdk_directory}\" /winsdkversion ${windows_WindowsSDKVersion}")
set(CMAKE_C_FLAGS_INIT "${windows_driver_arguments}")
set(CMAKE_CXX_FLAGS_INIT "${windows_driver_arguments}")

set(CMAKE_C_COMPILER "${IOJ_CLANG_CL_EXECUTABLE}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${IOJ_CLANG_CL_EXECUTABLE}" CACHE FILEPATH "" FORCE)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL CACHE STRING "" FORCE)
set(IOJ_NATIVE_TOOLCHAIN clang-cl CACHE STRING "Native compiler toolchain" FORCE)
