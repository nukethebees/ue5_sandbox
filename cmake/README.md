# CMake builds

CMake builds native C++ and orchestrates UnrealBuildTool. Start with the
[setup requirements](../docs/build-and-test.md#setup), then prepare the worktree:

```powershell
coj prepare-worktree
```

Configure and build the DebugGame editor with its required native dependencies:

```powershell
cmake --preset debug-game
cmake --build --preset debug-game
```

For native-only work, configure `native` and build the target you need:

```powershell
cmake --preset native -DCMAKE_UNITY_BUILD=OFF
cmake --build --preset native --target <target>
```

See [build and test](../docs/build-and-test.md) for presets, focused tests, compiler
caching and build profiling; [build architecture](ARCHITECTURE.md) and
[CMake style](STYLE.md) cover build-layer maintenance.
