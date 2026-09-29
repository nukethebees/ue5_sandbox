# Developer tools

The maintainer installs stable tools explicitly under
`%LOCALAPPDATA%\NukeTheBees\<ToolName>\bin`, keeping each tool's complete runtime together.

| Central tool | Installation/update | Source version |
| --- | --- | --- |
| agent-task | `. .\dev.ps1`, then `install-agent-task` | Cargo.toml |
| jobserver | `agent-task install-central-tools` | CLI source |

Installers publish literal symlinks in `%NTB_APPDATA_LOCAL%\bin` (default
`%LOCALAPPDATA%\NukeTheBees\bin`). Add only that directory to PATH. Enable Windows Developer Mode before installing,
or grant your account the **Create symbolic links** right and sign out/in. Installers check
link creation before building or updating tools and refuse to overwrite unrelated files.
The two installers share only `install/ToolLinks.ps1`: destination selection, symlink preflight,
and collision-safe publication. Revision-local Rust tools use the workspace's pinned Rust toolchain.

`agent-task jobs` accesses the per-user jobs board. Its installer shuts down an empty board,
copies the binaries, registers the logon task, and verifies startup. See [jobserver](jobserver/README.md).
Agents assume central tools are installed, report missing commands, and never install/update them
or build local fallbacks. `--version` is for manual diagnosis; bump the source version when shipping changes.

Use stock Codex. This repository does not build or install it.

`agent-task prepare-worktree` clears output, updates submodules,
generates presets, disables Live Coding in existing
`Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`, and generates code.
Missing settings are left absent. Preparation never installs tools or performs a broad build.
See the [Rust guide](rust/README.md) for AgentTask installation and feature Git operations.
After preparation, `agent-task install-central-tools` configures native and installs jobserver.

`agent-task format [--all|--changed|--staged] [--jobs N|-j N] [--verbose]` reads formatting roots,
extensions, and exclusions from the current checkout's `.code-format.json`. Styling stays in
`.clang-format`. CMake formatting and Unreal builds invoke `agent-task` from PATH. Use
`cmake --workflow --preset format-code` for changed C++ files and CMake's Unreal presets for
builds; `agent-task unreal-build` supplies the small build-script invocation boundary.

CMake builds revision-local tools privately in each configuration's output directory:

- `game-package-tools`: exact package and asset expectations.
- `native-binary-tools`: native object and mimalloc symbol-prefix integration.
- `benchmark-tools`: revision-specific presets, executable locations, scenarios, and baselines.
  PowerShell benchmark commands build `benchmark-tools-host` through CMake on demand.

All three are Rust crates built under `out/build/<configuration>/rust-tools/release/`.
They launch domain subprocesses directly. The caller manages jobs-board admission and cancellation;
the tools do not maintain process trees or scheduling state.

PowerShell worktree navigation uses read-only `git worktree list --porcelain -z`.
Optional `tracy-benchmark-compare` has a separate explicit install; see [profiling](../docs/profiling.md).
Other tools: [layout planner](layout_planner/README.md), [image lab](image_lab/README.md), and
[LLVM checks](../docs/clang-tidy.md). Run focused tests for affected tools.
