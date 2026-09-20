# Developer tools

`tools/` contains standalone utilities shared by repository workflows.

Run `ctools` when a direct developer command needs a staged executable in `tools/bin`. CMake
workflows build their own configuration-local C# host-tool outputs on demand and never depend on
the shared staging directory.

Run `cmake --workflow --preset tool-tests` when changing a standalone tool, its tests, directly
consumed interfaces/configuration, or shared tool/build infrastructure. Ordinary game, runtime,
and native validation does not run this suite.

- `jobserver/` is the canonical per-user coordinator for build, Editor, test, commandlet, and
  benchmark resource claims. See its [detailed README](jobserver/README.md).
- `perf/` contains performance tooling integrated into the root CMake project.
- `GitTools/` is a small C# executable for Git worktree discovery. Build the complete C# tooling
  workspace with `dotnet build tools/Tools.slnx` or `ctools` after loading `dev.ps1`.
- `BenchmarkTools/` owns reusable benchmark orchestration. Its staged executable runs a native S7
  workload as `tools/bin/BenchmarkTools.exe native-simulation --level <path> --seconds <value>`;
  it performs the configured CMake build, then acquires the exclusive benchmark and machine lease.
  The native benchmark PowerShell entry points build and stage this project on demand when it is
  absent, without building the rest of the standalone tools or requiring Unreal setup.
- `NativeBinaryTools/` inspects native object files for build integration checks. Its staged
  executable can be run as `tools/bin/NativeBinaryTools.exe mimalloc-symbols <generate|verify> ...`.
  Native CMake builds use a configuration-local copy built on demand.
- `AgentGit/` is the repository-aware, policy-enforcing Git interface intended for autonomous
  agents. Its repository build output is deliberately not trusted for mutations; use
  `install-agent-git` to create the canonical per-user installation described in
  [the agent-git documentation](../docs/agent-git.md). The installer builds and validates in a
  private per-install output directory rather than installing from shared `tools/bin` state.
- `CodeFormatTools/` is the C# formatter for repository C++ and shader files. CMake builds it for
  the `format-code` and `format-all-code` workflows; it can also be run directly as
  `tools/bin/CodeFormatTools.exe [--all|--changed|--staged] [--jobs N|-j N] [--verbose]` after
  `ctools`. Formatting runs concurrently by default with half the logical processor count, capped
  at 16 jobs; use `--jobs 1` for sequential execution.
- `ArchitectureChecks/` validates repository architecture invariants. CMake builds its SpaceGame
  layer check on demand for `check-space-game-layers`; its staged executable can be run directly as
  `tools/bin/ArchitectureChecks.exe --root <path>` after `ctools`. Its advisory module-migration
  audit is `tools/bin/ArchitectureChecks.exe module-migration --root <path> [--baseline <revision>]
  [--old-module <module>] [--plugin-module <module> ...]`. It defaults to auditing migrations from
  `Sandbox` into `ShooterGame` and `SandboxGameShared`; each specified plugin module is added to
  that default set, with duplicates removed. The audit reports review findings but exits successfully
  unless its arguments, repository access, or read-only Git queries fail. Run `ArchitectureChecks.exe module-migration
  --help` for its command summary.
- `GamePackageTools/` verifies archived game packages through the `verify-package` CMake target.
  Its staged executable accepts `--project-root`, `--package-root`, `--unreal-pak`,
  `--verification-directory`, and `--configuration`.
- `SetLiveCodingDisabled/` is a small C# executable for disabling Live Coding in saved editor
  settings while preserving the file's encoding and line endings.
- `UnrealBuildTools/` is a thin C# executable that validates paths, scopes the native toolchain
  environment, invokes UBT, and propagates its result. UBT remains solely responsible for target
  receipts, module manifests, and BuildIds.

`Directory.Build.props` applies the shared target framework, nullable, implicit-using, warning,
analysis, and warnings-as-errors policy to every .NET project below `tools/`.

New standalone C# tools should use their own project and test project under this directory. C# owns
developer-tool validation, subprocess execution, filesystem work, jobserver integration, and
benchmark/report orchestration; PowerShell remains the interactive shell façade and Python remains
appropriate for plotting or scientific analysis.
Executable command projects opt into staging with `IsStandaloneTool=true`; their normal Debug and
Release output remains project-local, while the post-build target copies the complete runtime output
tree for the most recently built configuration into `tools/bin/` using the tool's unique executable
name. Test projects and class libraries are not staged. Add shared infrastructure only when more
than one tool needs it.

Normal development uses the jobserver indirectly through CMake workflows and `dev.ps1`. Query its
current state with `get-jobserver-state` after loading `dev.ps1`; see [Build and test](../docs/build-and-test.md)
and [Benchmarks](../docs/benchmarks.md) for the coordination rules. For a jobserver-aware Tracy
capture of a native level benchmark, see [Profiling](../docs/profiling.md).
