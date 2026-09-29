add_custom_target(generate-project-files
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "IOJ_NATIVE_TOOLCHAIN=${IOJ_NATIVE_TOOLCHAIN}"
    "${UE_GENERATE_PROJECT_FILES_SCRIPT}"
    ${UE_GENERATE_PROJECT_FILES_ARGUMENTS}
    "-Project=${IOJ_UPROJECT}"
    -Game
    -WaitMutex
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating Visual Studio project files for ${IOJ_UPROJECT}"
  USES_TERMINAL
  VERBATIM
)
set_property(TARGET generate-project-files PROPERTY FOLDER "Sandbox/Unreal/Maintenance")
