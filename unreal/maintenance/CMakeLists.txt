add_custom_target(generate-project-files
  COMMAND "${SANDBOX_SET_LIVE_CODING_DISABLED_TOOL}"
    --settings-path
    "${PROJECT_SOURCE_DIR}/Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini"
  COMMAND ${UE_JOBSERVER_COMMAND_PREFIX}
    "${CMAKE_COMMAND}" -E env
    "SANDBOX_NATIVE_TOOLCHAIN=${SANDBOX_NATIVE_TOOLCHAIN}"
    "${UE_GENERATE_PROJECT_FILES_SCRIPT}"
    "-Project=${SANDBOX_UPROJECT}"
    -Game
    -WaitMutex
  DEPENDS sandbox-set-live-coding-disabled-preflight
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Generating Visual Studio project files for ${SANDBOX_UPROJECT}"
  USES_TERMINAL
  VERBATIM
)
set_property(TARGET generate-project-files PROPERTY FOLDER "Sandbox/Unreal/Maintenance")

find_program(DOTNET_EXECUTABLE NAMES dotnet REQUIRED)
add_test(
  NAME Sandbox.CSharpTools
  COMMAND "${DOTNET_EXECUTABLE}" test "${PROJECT_SOURCE_DIR}/tools/Tools.slnx" -m:1
)
set_tests_properties(Sandbox.CSharpTools PROPERTIES
  LABELS "unit;all"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)
