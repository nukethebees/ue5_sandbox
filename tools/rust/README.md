# Rust developer-tool experiments

This Cargo workspace holds small native developer-tool experiments. It does not establish Rust as
the replacement for every C#, Python, or C++ tool.
Rustup selects the pinned toolchain from `rust-toolchain.toml` when commands run in this directory.

## agent-task

Begin each new task from anywhere in its Git worktree with:

```powershell
agent-task prepare-worktree
```

This removes the worktree-root `out`, synchronizes and initializes/updates recursive submodules,
then regenerates CMake presets and code. It does not perform a broad project/test build; build
only the targets relevant to the task afterward. It stops on the first failure and streams
subprocess output. Git, Python, CMake, and the repository build prerequisites must be available.

Preparation checks for the canonical jobserver before clearing output. Install/update central
per-user build tools explicitly when needed:

```powershell
agent-task install-central-tools
```

This synchronizes and initializes/updates recursive submodules, generates presets, configures
`native`, then runs the canonical `install-jobserver` and
`install-set-live-coding-disabled` CMake targets, stopping on failure. It does not install AgentGit;
that remains separately maintainer-controlled through `install-agent-git`.

The maintainer installs/updates `agent-task` itself from the repository root with:

```powershell
. .\dev.ps1
install-agent-task
```

This runs the package tests, installs only `agent-task` with the pinned Rust toolchain, and
smoke-tests the installed executable with `--help`. The maintainer manages PATH; add
`%LOCALAPPDATA%\NukeTheBees\agent-task\bin`. Agents assume `agent-task` is already available.

The executable lives outside `out`, so it can clear build output while running. For development,
build and test directly from `tools/rust`; these tests are not part of broad CTest bundles:

```powershell
cargo build --release --package agent-task --locked
cargo test --package agent-task --locked
```

The CLI tests use temporary Git repositories and small Python/CMake fixtures; they do not build
the game or clear this repository's output.

## set-live-coding-disabled

`set-live-coding-disabled` is the first experiment. Build and test it with:

```powershell
Set-Location tools/rust
cargo build --release --package set-live-coding-disabled
cargo test --package set-live-coding-disabled
```

The canonical Windows installation is:

```text
%LOCALAPPDATA%\NukeTheBees\bin\set-live-coding-disabled.exe
```

From a configured repository build directory, install or update it explicitly with:

```powershell
cmake --build --preset <preset> --target install-set-live-coding-disabled
```

CMake call sites use that canonical path and do not fall back to repository build outputs. The
installer stages and smoke-tests a replacement before atomically activating it. It treats the
directory as shared per-user state.

A future jobserver installation barrier can admit this installer only after tool users have drained,
then stage, validate, and activate the replacement before allowing queued work to resume. This
experiment does not implement that barrier. The installed executable currently has an existence
check only; a future barrier must also account for worktrees expecting different tool versions.
