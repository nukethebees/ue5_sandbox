include("${PROJECT_SOURCE_DIR}/cmake/unreal_arguments.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/unreal.cmake")

set(plugin_fixture "${CMAKE_CURRENT_BINARY_DIR}/plugins.uproject")
file(WRITE "${plugin_fixture}" [=[{"Plugins":[
  {"Name":"EnabledPlugin","Enabled":true},
  {"Name":"SandboxExperiments","Enabled":false}
]}]=])
sandbox_project_enables_plugin(plugin_enabled "${plugin_fixture}" EnabledPlugin)
sandbox_assert_equal("explicitly enabled plugin" "${plugin_enabled}" ON)
sandbox_project_enables_plugin(plugin_enabled "${plugin_fixture}" SandboxExperiments)
sandbox_assert_equal("explicitly disabled plugin" "${plugin_enabled}" OFF)
sandbox_project_enables_plugin(plugin_enabled "${plugin_fixture}" MissingPlugin)
sandbox_assert_equal("absent opt-in plugin" "${plugin_enabled}" OFF)

sandbox_make_automation_filter_expression(automation_expression
  Sandbox.UnitTests Sandbox.LevelTests.PlayerInputSmoke)
sandbox_assert_equal("automation filters preserve order"
  "${automation_expression}"
  "StartsWith:Sandbox.UnitTests+StartsWith:Sandbox.LevelTests.PlayerInputSmoke")
sandbox_make_automation_exec_commands(automation_commands "${automation_expression}")
sandbox_assert_equal("automation preserves the startup discovery delay"
  "${automation_commands}"
  "-ExecCmds=Automation RunTests ${automation_expression}; Quit")

sandbox_make_unreal_cook_arguments(cook_arguments
  [=[C:\Engine Root\UnrealEditor-Cmd.exe]=]
  [=[C:\Artifacts Root\cook\Windows]=])
sandbox_assert_list_equal("cook arguments preserve Windows paths" "${cook_arguments}"
  [=[-cook;-skipstage;-unrealexe=C:\Engine Root\UnrealEditor-Cmd.exe;-ddc=NoZenLocalFallback;-cookoutputdir=C:\Artifacts Root\cook\Windows]=])
sandbox_make_unreal_container_arguments(container_arguments
  [=[C:\Artifacts Root\cook]=] [=[C:\Artifacts Root\stage]=])
sandbox_assert_list_equal("container arguments preserve Windows paths" "${container_arguments}"
  [=[-pak;-iostore;-compressed;-prereqs;-cookoutputdir=C:\Artifacts Root\cook;-stagingdirectory=C:\Artifacts Root\stage]=])
