# Developer tools

The maintainer installs stable tools explicitly under
`%LOCALAPPDATA%\NukeTheBees\<ToolName>\bin`, keeping each tool's complete runtime together.

| Central tool | Installation/update | Source version |
| --- | --- | --- |
| agent-task | `. .\dev.ps1`, then `install-agent-task` | Cargo.toml |
| jobserver | `agent-task install-central-tools` | CLI source |
| UnrealBuildTools | `agent-task install-central-tools` | csproj Version |
| CodeFormatTools | `agent-task install-central-tools` | csproj Version |

The maintainer manages PATH for ordinary tools. Internal jobserver calls use
`%LOCALAPPDATA%\NukeTheBees\jobserver\bin\jobserver.exe`; its separate installer preserves
staged validation, shutdown, startup verification, and rollback. See [jobserver](jobserver/README.md).
Agents assume central tools are installed, report missing commands, and never install/update them
or build local fallbacks. `--version` is for manual diagnosis; bump the source version when shipping changes.

`agent-task prepare-worktree` checks for the canonical jobserver, clears output, updates submodules,
generates presets, disables Live Coding in existing
`Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`, and generates code.
Missing settings are left absent. Preparation never installs tools or performs a broad build.
See the [Rust guide](rust/README.md) for AgentTask installation and feature Git operations.
Obsolete `set-live-coding-disabled` installations can be deleted manually, including old copies
under `%LOCALAPPDATA%\NukeTheBees\bin`.

For one C# tool, run `pwsh -NoProfile -File tools/install/Install-CentralDotnetTool.ps1 -ToolName
UnrealBuildTools` (or `CodeFormatTools`). The installer publishes privately, runs focused project
tests and candidate `--version`, then replaces the tool's bin directory. Failed validation leaves
the existing installation intact. Use `-InstallRoot <private-tool-root>` for isolated testing.

`CodeFormatTools [--all|--changed|--staged] [--jobs N|-j N] [--verbose]` reads formatting roots,
extensions, and exclusions from the current checkout's `.code-format.json`. Styling stays in
`.clang-format`. CMake formatting and Unreal builds invoke the central commands from PATH.

CMake builds revision-local tools privately under `out/build/<configuration>/host-tools/`:

- ArchitectureChecks: checked-out module and dependency policy.
- GamePackageTools: exact package and asset expectations.
- NativeBinaryTools: native object and mimalloc symbol-prefix integration.
- BenchmarkTools: revision-specific presets, executable locations, scenarios, and baselines.
  PowerShell benchmark commands build `benchmark-tools-host` through CMake on demand.

PowerShell worktree navigation uses read-only `git worktree list --porcelain -z`.
Optional `tracy-benchmark-compare` has a separate explicit install; see [profiling](../docs/profiling.md).
Other tools: [layout planner](layout_planner/README.md), [image lab](image_lab/README.md), and
[LLVM checks](../docs/clang-tidy.md). Run focused tests for affected tools.
