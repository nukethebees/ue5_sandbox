# Developer tools

`tools/` contains standalone utilities shared by repository workflows.

- `jobserver/` is the canonical per-user coordinator for build, Editor, test, commandlet, and
  benchmark resource claims. See its [detailed README](jobserver/README.md).
- `perf/` contains performance tooling integrated into the root CMake project.
- `GitTools/` is a small C# executable for Git worktree discovery. Build the complete C# tooling
  workspace with `dotnet build tools/Tools.slnx` or `ctools` after loading `dev.ps1`.

New standalone C# tools should use their own project and test project under this directory. Add
shared infrastructure only when more than one tool needs it.

Normal development uses the jobserver indirectly through CMake workflows and `dev.ps1`. Query its
current state with `get-jobserver-state` after loading `dev.ps1`; see [Build and test](../docs/build-and-test.md)
and [Benchmarks](../docs/benchmarks.md) for the coordination rules. For a jobserver-aware Tracy
capture of a native level benchmark, see [Profiling](../docs/profiling.md).
