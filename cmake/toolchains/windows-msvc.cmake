if(NOT CMAKE_HOST_WIN32)
  message(FATAL_ERROR "The MSVC toolchain supports only Windows hosts.")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../msvc_environment.cmake")

find_program(SANDBOX_NINJA_EXECUTABLE NAMES ninja REQUIRED)
set(CMAKE_MAKE_PROGRAM "${SANDBOX_NINJA_EXECUTABLE}" CACHE FILEPATH "" FORCE)

if(NOT DEFINED ENV{VCToolsInstallDir})
  find_program(SANDBOX_VSWHERE_EXECUTABLE NAMES vswhere vswhere.exe
    PATHS "C:/Program Files (x86)/Microsoft Visual Studio/Installer"
    REQUIRED)

  execute_process(
    COMMAND "${SANDBOX_VSWHERE_EXECUTABLE}" -latest -products *
      -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64
      -property installationPath
    OUTPUT_VARIABLE visual_studio_root
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE vswhere_result
  )
  if(NOT vswhere_result EQUAL 0 OR visual_studio_root STREQUAL "")
    message(FATAL_ERROR "No Visual Studio installation with the C++ toolchain was found.")
  endif()

  set(vsdevcmd "${visual_studio_root}/Common7/Tools/VsDevCmd.bat")
  execute_process(
    COMMAND cmd.exe /d /v:on /c call "${vsdevcmd}"
      -no_logo -arch=x64 -host_arch=x64 >nul
      && echo SANDBOX_MSVC_PATH=!PATH!
      && echo SANDBOX_MSVC_INCLUDE=!INCLUDE!
      && echo SANDBOX_MSVC_LIB=!LIB!
      && echo SANDBOX_MSVC_LIBPATH=!LIBPATH!
      && echo SANDBOX_MSVC_VCTOOLSINSTALLDIR=!VCToolsInstallDir!
      && echo SANDBOX_MSVC_VCINSTALLDIR=!VCINSTALLDIR!
      && echo SANDBOX_MSVC_VSINSTALLDIR=!VSINSTALLDIR!
      && echo SANDBOX_MSVC_WINDOWSSDKDIR=!WindowsSdkDir!
      && echo SANDBOX_MSVC_WINDOWSSDKVERSION=!WindowsSDKVersion!
      && echo SANDBOX_MSVC_UNIVERSALCRTSDKDIR=!UniversalCRTSdkDir!
      && echo SANDBOX_MSVC_UCRTVERSION=!UCRTVersion!
    OUTPUT_VARIABLE msvc_environment
    RESULT_VARIABLE vsdevcmd_result
  )
  if(NOT vsdevcmd_result EQUAL 0)
    message(FATAL_ERROR "Failed to initialise the Visual Studio C++ build environment.")
  endif()

  foreach(environment_name IN ITEMS
      PATH INCLUDE LIB LIBPATH VCTOOLSINSTALLDIR VCINSTALLDIR VSINSTALLDIR
      WINDOWSSDKDIR WINDOWSSDKVERSION UNIVERSALCRTSDKDIR UCRTVERSION)
    sandbox_get_msvc_environment_value(environment_value
      "${msvc_environment}" "${environment_name}")
    set(msvc_${environment_name} "${environment_value}")
    set(ENV{${environment_name}} "${environment_value}")
  endforeach()

  cmake_path(APPEND msvc_VCTOOLSINSTALLDIR bin Hostx64 x64 cl.exe
    OUTPUT_VARIABLE msvc_cl)
else()
  find_program(msvc_cl NAMES cl REQUIRED)
endif()

if(NOT EXISTS "${msvc_cl}")
  message(FATAL_ERROR "Visual Studio setup did not provide cl.exe at '${msvc_cl}'.")
endif()

set(msvc_include_directories "$ENV{INCLUDE}")
set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES "${msvc_include_directories}"
  CACHE STRING "MSVC system include directories" FORCE)
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES "${msvc_include_directories}"
  CACHE STRING "MSVC system include directories" FORCE)

set(msvc_library_directories "$ENV{LIB}")
sandbox_make_msvc_library_linker_flags(msvc_linker_flags ${msvc_library_directories})
set(CMAKE_EXE_LINKER_FLAGS_INIT "${msvc_linker_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${msvc_linker_flags}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${msvc_linker_flags}")

set(CMAKE_C_COMPILER "${msvc_cl}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${msvc_cl}" CACHE FILEPATH "" FORCE)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL CACHE STRING "" FORCE)
set(SANDBOX_NATIVE_TOOLCHAIN msvc CACHE STRING "Native compiler toolchain" FORCE)
