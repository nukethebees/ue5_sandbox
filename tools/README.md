# Developer tools

| Task | Entry point |
| --- | --- |
| Prepare a worktree or manage a feature branch | [coj](rust/crates/coj/README.md) |
| Measure a revision | [Benchmarks](../docs/benchmarks.md) |
| Inspect or edit Lispb memory layouts | [Layout Planner](layout_planner/README.md) |
| Generate PNG textures | [Image Lab](image_lab/README.md) |
| Build the project LLVM checks | [LLVM](llvm/README.md) |

The maintainer installs shared tools under `%IOJ_ROOT%\tools`. CMake builds the revision's packaging, native binary, and benchmark tools on demand. Windows tests use `%IOJ_ROOT%\tmp\<worktree-name>`. [Packaging scripts](../PowerShell/README.md)
