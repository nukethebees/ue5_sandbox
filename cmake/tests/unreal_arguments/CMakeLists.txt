include("${PROJECT_SOURCE_DIR}/cmake/unreal_arguments.cmake")

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
