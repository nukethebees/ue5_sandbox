# PowerShell developer commands

`dev.ps1` is the returning-developer entry point. Dot-source it so its navigation and build
functions remain available in the current session:

```powershell
. .\dev.ps1
dev-help
```

The maintainer runs `install-agent-task` to test and install only the Rust `agent-task` CLI,
then manages its PATH entry. On a fresh setup, `agent-task install-central-tools` installs the
canonical per-user build tools in separate `%LOCALAPPDATA%\NukeTheBees\<ToolName>\bin`
directories. Ordinary tools use PATH.
Use `--version` for manual source/install comparison.
Missing tools require maintainer action, and `csetup` never installs them. Use `agent-task git`
for its documented subset of feature Git operations; see `agent-task git --help`.
Report unsupported operations instead of bypassing it.
Agents assume the CLI is installed and begin tasks with `agent-task prepare-worktree`, which also
disables Live Coding once if saved Editor settings exist.

`cwt`/`cwb` discovery and completion use read-only Git directly, including linked worktrees.
`Navigation.ps1` provides `croot`, `cwt`, `cwb`, `cplugin`, and `ctests`. `UnrealBuild.ps1` provides
`cbuild`, `csetup`, `cplay`, `cprojectfiles`, `integrate-feature`, and jobserver/UBT state helpers.
After the user authorizes a ready feature, request an exclusive scheduler ticket separately.
`integrate-feature` invokes privileged `agent-task integrate`: pinned rebase, cheap Git
sanity checks, atomic dev promotion, worktree refresh, and feature cleanup. Complete relevant
validation before integration. A conflicting final rebase is aborted; resolve with
`agent-task git rebase dev` and retry with a new ticket after validation. Use `-KeepBranch`
to retain the integrated feature branch. Persistent devN worktrees return to their home branches;
other worktrees detach at the integrated feature tip before branch deletion.

`cbuild` defaults to the native test workflow; `csetup native` prepares native-only prerequisites,
while `cplay` prepares and builds playable Editor configurations. Use `cbuild tool-tests` for
standalone developer-tool validation when that scope is affected.

The remaining scripts implement build safety, packaging, and project-file generation. Treat them
as implementation details unless a documented workflow calls for one directly. In particular, use
CMake workflows rather than calling UBT or its batch wrappers yourself.
`TestUnrealEditorConfigurationTransition.ps1` is the focused DebugGame-to-Development-to-DebugGame
module-loading regression. Request an exclusive ticket for the whole sequence before invoking it;
the script runs ordinary CMake and CTest commands.

See [Build and test](../docs/build-and-test.md) for everyday commands and [the CMake guide](../cmake/README.md)
for how those commands are coordinated.
