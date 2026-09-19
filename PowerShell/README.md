# PowerShell developer commands

`dev.ps1` is the returning-developer entry point. Dot-source it so its navigation and build
functions remain available in the current session:

```powershell
. .\dev.ps1
dev-help
```

`Navigation.ps1` provides `croot`, `cwt`, `cwb`, `cplugin`, and `ctests`. `UnrealBuild.ps1` provides
`cbuild`, `csetup`, `cplay`, `cprojectfiles`, and jobserver/UBT state helpers. `cbuild` defaults to
the native test workflow; `csetup native` prepares native-only prerequisites, while `cplay` prepares
and builds playable Editor configurations.

The remaining scripts implement build safety, packaging, project-file generation, and Live Coding
configuration. Treat them as implementation details unless a documented workflow calls for one
directly. In particular, use CMake workflows rather than calling UBT or its batch wrappers yourself.
`TestUnrealEditorConfigurationTransition.ps1` is the focused DebugGame-to-Development-to-DebugGame
module-loading regression. It holds the canonical engine resource exclusively across the sequence
and invokes only nested coordinated CMake and CTest entry points.

See [Build and test](../docs/build-and-test.md) for everyday commands and [the CMake guide](../cmake/README.md)
for how those commands are coordinated.
