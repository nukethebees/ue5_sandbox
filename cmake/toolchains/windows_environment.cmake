include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/../msvc_environment.cmake")

set(windows_settings_file "${CMAKE_CURRENT_LIST_DIR}/../../Config/DefaultEngine.ini")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${windows_settings_file}")
file(STRINGS "${windows_settings_file}" windows_settings_lines)
set(in_windows_settings FALSE)
foreach(line IN LISTS windows_settings_lines)
  if(line MATCHES "^\\[")
    if(line STREQUAL "[/Script/WindowsTargetPlatform.WindowsTargetSettings]")
      set(in_windows_settings TRUE)
    else()
      set(in_windows_settings FALSE)
    endif()
  elseif(in_windows_settings AND line MATCHES "^(CompilerVersion|WindowsSDKVersion)=([0-9.]+)$")
    set(windows_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
  endif()
endforeach()
if(NOT windows_CompilerVersion OR NOT windows_WindowsSDKVersion)
  message(FATAL_ERROR "Pin CompilerVersion and WindowsSDKVersion in ${windows_settings_file}.")
endif()

find_program(IOJ_VSWHERE_EXECUTABLE NAMES vswhere vswhere.exe
  PATHS "C:/Program Files (x86)/Microsoft Visual Studio/Installer"
  REQUIRED)
execute_process(
  COMMAND "${IOJ_VSWHERE_EXECUTABLE}" -products *
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64
    -property installationPath
  OUTPUT_VARIABLE visual_studio_roots
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE vswhere_result
)
if(NOT vswhere_result EQUAL 0)
  message(FATAL_ERROR "Failed to locate Visual Studio C++ installations.")
endif()
string(REPLACE "\r" "" visual_studio_roots "${visual_studio_roots}")
string(REPLACE "\n" ";" visual_studio_roots "${visual_studio_roots}")
set(visual_studio_root "")
foreach(candidate IN LISTS visual_studio_roots)
  if(EXISTS "${candidate}/VC/Tools/MSVC/${windows_CompilerVersion}/bin/Hostx64/x64/cl.exe")
    set(visual_studio_root "${candidate}")
    break()
  endif()
endforeach()
if(visual_studio_root STREQUAL "")
  message(FATAL_ERROR
    "Install the MSVC ${windows_CompilerVersion} x64 build tools pinned in ${windows_settings_file}.")
endif()

# Reuse the selected environment in CMake's nested compiler checks.
file(TO_CMAKE_PATH "$ENV{VCToolsInstallDir}" inherited_toolchain_directory)
file(TO_CMAKE_PATH "${visual_studio_root}/VC/Tools/MSVC/${windows_CompilerVersion}" expected_toolchain_directory)
file(TO_CMAKE_PATH "$ENV{WindowsSDKVersion}" inherited_sdk_version)
get_property(in_try_compile GLOBAL PROPERTY IN_TRY_COMPILE)
if(NOT in_try_compile
    OR NOT inherited_toolchain_directory STREQUAL expected_toolchain_directory
    OR NOT inherited_sdk_version STREQUAL windows_WindowsSDKVersion)
  # VsDevCmd appends inherited search paths; discard paths from other toolsets.
  set(ENV{INCLUDE} "")
  set(ENV{LIB} "")
  set(ENV{LIBPATH} "")
  set(ENV{EXTERNAL_INCLUDE} "")
  set(vsdevcmd "${visual_studio_root}/Common7/Tools/VsDevCmd.bat")
  execute_process(
    COMMAND cmd.exe /d /v:on /c call "${vsdevcmd}"
      -no_logo -arch=x64 -host_arch=x64
      "-vcvars_ver=${windows_CompilerVersion}" "-winsdk=${windows_WindowsSDKVersion}" >nul
      && echo IOJ_MSVC_PATH=!PATH!
      && echo IOJ_MSVC_INCLUDE=!INCLUDE!
      && echo IOJ_MSVC_EXTERNAL_INCLUDE=!EXTERNAL_INCLUDE!
      && echo IOJ_MSVC_LIB=!LIB!
      && echo IOJ_MSVC_LIBPATH=!LIBPATH!
      && echo IOJ_MSVC_VCTOOLSINSTALLDIR=!VCToolsInstallDir!
      && echo IOJ_MSVC_VCINSTALLDIR=!VCINSTALLDIR!
      && echo IOJ_MSVC_VSINSTALLDIR=!VSINSTALLDIR!
      && echo IOJ_MSVC_WINDOWSSDKDIR=!WindowsSdkDir!
      && echo IOJ_MSVC_WINDOWSSDKVERSION=!WindowsSDKVersion!
      && echo IOJ_MSVC_UNIVERSALCRTSDKDIR=!UniversalCRTSdkDir!
      && echo IOJ_MSVC_UCRTVERSION=!UCRTVersion!
    OUTPUT_VARIABLE msvc_environment
    RESULT_VARIABLE vsdevcmd_result
  )
  if(NOT vsdevcmd_result EQUAL 0)
    message(FATAL_ERROR
      "Failed to initialise MSVC ${windows_CompilerVersion} and Windows SDK ${windows_WindowsSDKVersion}. "
      "Install the pinned versions through Visual Studio Installer.")
  endif()
endif()

foreach(environment_name IN ITEMS
    PATH INCLUDE EXTERNAL_INCLUDE LIB LIBPATH VCTOOLSINSTALLDIR VCINSTALLDIR VSINSTALLDIR
    WINDOWSSDKDIR WINDOWSSDKVERSION UNIVERSALCRTSDKDIR UCRTVERSION)
  if(DEFINED msvc_environment)
    sandbox_get_msvc_environment_value(environment_value
      "${msvc_environment}" "${environment_name}")
  else()
    set(environment_value "$ENV{${environment_name}}")
  endif()
  set(msvc_${environment_name} "${environment_value}")
  set(ENV{${environment_name}} "${environment_value}")
endforeach()

file(TO_CMAKE_PATH "${msvc_VCTOOLSINSTALLDIR}" msvc_toolchain_directory)
file(TO_CMAKE_PATH "${msvc_WINDOWSSDKDIR}" msvc_sdk_directory)
file(TO_CMAKE_PATH "${msvc_WINDOWSSDKVERSION}" msvc_sdk_version)
if(NOT msvc_toolchain_directory STREQUAL expected_toolchain_directory
    OR NOT msvc_sdk_version STREQUAL windows_WindowsSDKVersion)
  message(FATAL_ERROR "Visual Studio setup did not select the project's pinned MSVC and SDK versions.")
endif()
cmake_path(APPEND msvc_toolchain_directory bin Hostx64 x64 cl.exe OUTPUT_VARIABLE msvc_cl)
if(NOT EXISTS "${msvc_cl}")
  message(FATAL_ERROR "Visual Studio setup did not provide cl.exe at '${msvc_cl}'.")
endif()

set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES "${msvc_INCLUDE}"
  CACHE STRING "MSVC system include directories" FORCE)
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES "${msvc_INCLUDE}"
  CACHE STRING "MSVC system include directories" FORCE)
sandbox_make_msvc_library_linker_flags(msvc_linker_flags ${msvc_LIB})
set(CMAKE_EXE_LINKER_FLAGS_INIT "${msvc_linker_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${msvc_linker_flags}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${msvc_linker_flags}")
message(STATUS "Windows toolchain: MSVC ${windows_CompilerVersion}, SDK ${windows_WindowsSDKVersion}")
