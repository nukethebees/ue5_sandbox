# SandboxHost on Windows

Set `UE_ROOT` to the patched UE 5.8.2 source build. On another Windows workstation, register that checkout under the `EngineAssociation` identifier in `Sandbox.uproject` so opening the project selects the same engine. Cook the content once in Development, then build the `stage` target for Win64 Development, DebugGame, or Shipping. All configurations share the Development cook. The player launcher lives at `stage/Windows/Sandbox/Binaries/Win64/SandboxHost.exe` beside the configuration's Unreal game DLL. Run `coj run-staged` for the Development stage. The editor's **Play Mission** button uses the path published by staging in `Saved/SandboxHost.path`.

Use `--display-mode windowed`, `borderless` or `headless` to override saved preferences for one run. Borderless fills the current monitor. Headless runs without a visible window or audio.

Run `SandboxHost --display-mode windowed --window-size 1280x720` for an initial client area of 1280 by 720 pixels. Windowed mode without an explicit size caps the initial client area at those dimensions. With no display override, Options offers windowed and monitor-sized borderless modes. Options saves an explicitly changed display setting after confirmation. Revert or the 15-second timeout restores the previous window geometry.

Use `--mission LevelScripts/FleetOverview/FleetOverview.scm --exit-when-ready` for a headless mission smoke check. Relative mission paths start at the staged project. Use `--user-dir DIRECTORY` for isolated preferences and saves. Run the staged integration tests with `ctest --test-dir out/build/development -L sandbox-host --output-on-failure`.
