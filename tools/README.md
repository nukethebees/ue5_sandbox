# Developer tools

Stable command-line tools are installed explicitly by the maintainer, one complete runtime per
`%LOCALAPPDATA%\NukeTheBees\<ToolName>\bin`. The maintainer manages PATH; installers only print
which directory to add. Agents invoke executable names from PATH, report missing commands, and
never install/update them automatically. `--version` supports manual comparison with this checkout.
There is no automatic version enforcement or worktree synchronization.
Bump the source-controlled version when shipping tool changes; build timestamps are omitted.

| Central tool | Installation/update | Source version |
| --- | --- | --- |
| agent-task | `. .\dev.ps1`, then `install-agent-task` | Cargo.toml |
| jobserver | `agent-task install-central-tools` | CLI source (`version` also remains available) |
| set-live-coding-disabled | `agent-task install-central-tools` | Cargo.toml |
| UnrealBuildTools | `agent-task install-central-tools` | csproj Version |
| CodeFormatTools | `agent-task install-central-tools` | csproj Version |
| tracy-benchmark-compare | Explicit CMake component install; [profiling guide](../docs/profiling.md) | tools/perf/CMakeLists.txt |

`agent-task prepare-worktree` only checks prerequisites, clears output, updates submodules, and
regenerates presets/code. It never installs central tools or performs a broad task-start build.
See the [Rust guide](rust/README.md) for AgentTask bootstrap and feature Git operations.

For one C# tool, use `pwsh -NoProfile -File tools/install/Install-CentralDotnetTool.ps1 -ToolName
UnrealBuildTools` (or `CodeFormatTools`). The matching CMake `install-<ToolName>` target is also
available. The installer publishes privately, runs focused tests, smoke-tests version/arguments,
and replaces the entire bin directory only after validation. Failed validation leaves the previous
installation intact; failed activation restores it. `-InstallRoot <private-tool-root>` supports
isolated testing. Source edits never change the installed copy. DLLs and runtime metadata stay
with their own tool. No installer changes PATH.

Jobserver retains its separate staged validation, drain/shutdown, startup verification, and rollback
lifecycle. Do not replace its binaries manually. See [jobserver](jobserver/README.md).

`CodeFormatTools [--all|--changed|--staged] [--jobs N|-j N] [--verbose]` uses `.code-format.json`
from the current Git worktree for roots, extensions, and excluded path components. Formatting
mechanics remain generic; clang-format styling remains in `.clang-format`. CMake formatting and
Unreal builds require the corresponding central executable on PATH and have no local fallback.
UnrealBuildTools remains a thin path/environment/launch wrapper; UBT owns receipts and BuildIds.

Revision-local C# tools are built privately by CMake under
`out/build/<configuration>/host-tools/<ToolName>/<Debug|Release>/`:

- ArchitectureChecks owns checked-out module/dependency policy (`check-space-game-layers`).
  Its private executable also provides advisory reports via `module-migration --help`.
- GamePackageTools owns exact package/asset expectations (`verify-package`).
- NativeBinaryTools owns native object/mimalloc symbol-prefix integration.
- BenchmarkTools owns revision-specific preset mappings, native benchmark locations, scenario/report
  commands, and baseline-worktree preparation. Centralizing it requires extracting those policies;
  that work is deferred. PowerShell benchmark commands build `benchmark-tools-host` through CMake,
  including when output already exists, so source edits are picked up.

There is no shared `tools/bin` staging or `ctools` command. Direct project builds keep their normal
project-local outputs; they do not update installed tools. GitSupport remains available to its
consumers. PowerShell worktree navigation uses read-only `git worktree list --porcelain -z`.

Other tools: [layout planner](layout_planner/README.md), [image lab](image_lab/README.md), and
[LLVM checks](../docs/clang-tidy.md). C# remains the usual stack for filesystem, subprocess, and
report tooling; focused Rust/native tools use their existing build paths.

`Directory.Build.props` supplies common .NET framework and warning policy. Run focused project
tests when changing a tool. Use `tool-tests` for shared tool infrastructure; native/game changes
should not run developer-tool suites without an affected dependency. Installer regression tests
use private roots and never activate live central installations.
