# Developer tools

`tools/` contains standalone utilities shared by repository workflows.

Run `ctools` after creating a worktree to build and stage standalone C# executables in `tools/bin`.
Workflows that directly use a staged executable require it to be present.

- `jobserver/` is the canonical per-user coordinator for build, Editor, test, commandlet, and
  benchmark resource claims. See its [detailed README](jobserver/README.md).
- `perf/` contains performance tooling integrated into the root CMake project.
- `GitTools/` is a small C# executable for Git worktree discovery. Build the complete C# tooling
  workspace with `dotnet build tools/Tools.slnx` or `ctools` after loading `dev.ps1`.
- `NativeBinaryTools/` inspects native object files for build integration checks. Its staged
  executable can be run as `tools/bin/NativeBinaryTools.exe mimalloc-symbols <generate|verify> ...`.
  Native CMake builds use a configuration-local copy built on demand, so they do not require a
  prior `ctools` run.
- `CodeFormatTools/` is the C# formatter for repository C++ and shader files. Run its staged
  executable through the `format-code` and `format-all-code` CMake workflows, or directly as
  `tools/bin/CodeFormatTools.exe [--all|--changed|--staged] [--jobs N|-j N] [--verbose]` after
  building tools. Formatting runs concurrently by default with half the logical processor count,
  capped at 16 jobs; use `--jobs 1` for sequential execution.
- `ArchitectureChecks/` validates repository architecture invariants. Its staged executable is
  invoked by `check-space-game-layers` as `tools/bin/ArchitectureChecks.exe --root <path>`.
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

New standalone C# tools should use their own project and test project under this directory.
Executable command projects opt into staging with `IsStandaloneTool=true`; their normal Debug and
Release output remains project-local, while the post-build target copies the complete runtime output
tree for the most recently built configuration into `tools/bin/` using the tool's unique executable
name. Test projects and class libraries are not staged. Add shared infrastructure only when more
than one tool needs it.

Normal development uses the jobserver indirectly through CMake workflows and `dev.ps1`. Query its
current state with `get-jobserver-state` after loading `dev.ps1`; see [Build and test](../docs/build-and-test.md)
and [Benchmarks](../docs/benchmarks.md) for the coordination rules. For a jobserver-aware Tracy
capture of a native level benchmark, see [Profiling](../docs/profiling.md).
