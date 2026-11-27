# Isle of Jebbers player for Windows

Set `UE_ROOT` to the [patched UE 5.8.4 source build](../../unreal/engine/README.md). Build the Development player and stage its content with `cmake --workflow --preset development-staged-game`. Run it with `coj run-staged`, or use **Play Mission** in the editor to launch the current level.

From the staged Win64 binaries directory, run `ioj.exe --level LevelScripts/FleetOverview/FleetOverview.scm`. Add `--exit-when-ready` for a headless launch or `--display-mode windowed --window-size 1280x720` for a window.

The CMake target `IojPlayer` builds `ioj.exe`. The executable owns the simulation and links
`native-presentation-bridge` to publish level initialisation data and resolved frame views.
Unreal consumes the contract in `native/presentation/include` and links the presentation
algorithms in `native/presentation/lib`. Frame columns are borrowed for one synchronous call.
SDL supplies keyboard, mouse and gamepad events. The player selects the menu, gameplay or benchmark/viewer context and resolves physical inputs into commands. Flight commands go to the simulation. Menu navigation and activation use the same commands for keyboard and controller. The main menu and benchmark HUD send button selections back to the player. Slate presents the controls, hit-tests pointer events and handles text entry. A focused binding prompt can request the physical input.

The Controls file stores bindings by name for compatibility with existing profiles. The player converts these names to typed mappings and keys when loading settings. Pause accepts keyboard keys, mouse buttons and gamepad buttons.
