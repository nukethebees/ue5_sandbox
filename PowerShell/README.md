# PowerShell developer commands

`dev.ps1` is the returning-developer entry point. Dot-source it so its navigation and build
functions remain available in the current session:

```powershell
. .\dev.ps1
dev-help
```

The maintainer runs `install-agent-task` to test and install only the Rust `agent-task` CLI,
then manages its PATH entry. Run `agent-task prepare-worktree` before
`agent-task install-central-tools`, which installs the per-user jobserver. Ordinary tools use PATH.
Use `--version` for manual source/install comparison.
Missing tools require maintainer action, and `csetup` never installs them. Use `agent-task git`
for its documented subset of feature Git operations; see `agent-task git --help`.
Report unsupported operations instead of bypassing it.
Agents assume the CLI is installed and begin tasks with `agent-task prepare-worktree`, which also
disables Live Coding once if saved Editor settings exist.

`cwt`/`cwb` discovery and completion use read-only Git directly, including linked worktrees.
`Navigation.ps1` provides `croot`, `cwt`, `cwb`, `cplugin`, and `ctests`. `UnrealBuild.ps1` provides
`cbuild`, `csetup`, `cplay`, `integrate-feature`, and `get-jobserver-state`. Generate IDE projects with `agent-task unreal project-files`.
After the user authorizes a ready feature, invoke `integrate-feature`.
`integrate-feature` invokes privileged `agent-task integrate`: pinned rebase, cheap Git
sanity checks, atomic dev promotion, worktree refresh, and feature cleanup. Complete relevant
validation before integration. A conflicting final rebase is aborted; resolve with
`agent-task git rebase dev` and retry after validation. Use `-KeepBranch`
to retain the integrated feature branch. Persistent devN worktrees return to their home branches;
other worktrees detach at the integrated feature tip before branch deletion.

`agent-task prepare-worktree` owns submodule initialization, presets, and code generation.
`cbuild` defaults to the native test workflow; `csetup` runs the Unreal dependency setup workflows
after preparation, while `cplay` sets up and builds playable Editor configurations. Use `cbuild tool-tests` for
standalone developer-tool validation when that scope is affected.

The remaining scripts compose CMake workflows for packaging and project-file generation. Treat them
as implementation details unless a documented workflow calls for one directly. In particular, use
CMake workflows rather than calling UBT or its batch wrappers yourself.
`tests/TestUnrealEditorConfigurationTransition.ps1` is the focused DebugGame-to-Development-to-DebugGame
module-loading regression. Use the [jobs-board protocol](../tools/jobserver/README.md) with an
exclusive ticket for the whole sequence: request, check until Ready, start, run, end.
The script runs ordinary CMake and CTest commands.

See [Build and test](../docs/build-and-test.md) for everyday commands and [the CMake guide](../cmake/README.md)
for how those commands are coordinated.
