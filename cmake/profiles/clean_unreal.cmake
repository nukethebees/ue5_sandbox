cmake_minimum_required(VERSION 4.4)

file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/../.." workspace)
if(NOT EXISTS "$ENV{UE_ROOT}/Engine/Build/BatchFiles/Clean.bat")
  message(FATAL_ERROR "Set UE_ROOT to the installed Unreal Engine directory")
endif()
execute_process(
  COMMAND "$ENV{UE_ROOT}/Engine/Build/BatchFiles/Clean.bat"
    SandboxEditor Win64 DebugGame "-Project=${workspace}/Sandbox.uproject" -WaitMutex
  WORKING_DIRECTORY "${workspace}"
  COMMAND_ERROR_IS_FATAL ANY
)
find_program(powershell_executable pwsh REQUIRED)
execute_process(
  COMMAND "${powershell_executable}" -NoProfile -File "${CMAKE_CURRENT_LIST_DIR}/unreal.ps1"
    -Operation Clean
  WORKING_DIRECTORY "${workspace}"
  COMMAND_ERROR_IS_FATAL ANY
)
