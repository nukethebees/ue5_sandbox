# Isle of Jebbers player for Windows

Set `UE_ROOT` to the [patched UE 5.8.4 source build](../../unreal/engine/README.md). Build the Development player and stage its content with `cmake --workflow --preset development-staged-game`. Run it with `coj run-staged`, or use **Play Mission** in the editor to launch the current level.

From the staged Win64 binaries directory, run `ioj.exe --level LevelScripts/FleetOverview/FleetOverview.scm`. Add `--exit-when-ready` for a headless launch or `--display-mode windowed --window-size 1280x720` for a window.
