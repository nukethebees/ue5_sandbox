# Developer tools

`tools/` contains standalone utilities shared by repository workflows.

Run `ctools` after creating a worktree to build and stage standalone C# executables in `tools/bin`.
Any workflow that uses one requires its staged executable to be present.

- `jobserver/` is the canonical per-user coordinator for build, Editor, test, commandlet, and
  benchmark resource claims. See its [detailed README](jobserver/README.md).
- `perf/` contains performance tooling integrated into the root CMake project.
- `GitTools/` is a small C# executable for Git worktree discovery. Build the complete C# tooling
  workspace with `dotnet build tools/Tools.slnx` or `ctools` after loading `dev.ps1`.
- `CodeFormatTools/` is the C# formatter for repository C++ and shader files. Run its staged
  executable through the `format-code` and `format-all-code` CMake workflows, or directly as
  `tools/bin/CodeFormatTools.exe [--all|--changed|--staged] [--verbose]` after building tools.
- `ArchitectureChecks/` validates repository architecture invariants. Its staged executable is
  invoked by `check-space-game-layers` as `tools/bin/ArchitectureChecks.exe --root <path>`.
- `GamePackageTools/` verifies archived game packages through the `verify-package` CMake target.
  Its staged executable accepts `--project-root`, `--package-root`, `--unreal-pak`,
  `--verification-directory`, and `--configuration`.
- `SetLiveCodingDisabled/` is a small C# executable for disabling Live Coding in saved editor
  settings while preserving the file's encoding and line endings.
- `UnrealBuildTools/` is a C# executable that invokes Unreal targets and verifies Editor module
  compatibility before and after Editor builds.

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
