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
`install-set-live-coding-disabled` CMake targets, stopping on failure.

The maintainer installs/updates `agent-task` itself from the repository root with:

```powershell
. .\dev.ps1
install-agent-task
```

This runs the package tests, installs only `agent-task` with the pinned Rust toolchain, and
smoke-tests the installed executable with `--help`. The maintainer manages PATH; add
`%LOCALAPPDATA%\NukeTheBees\agent-task\bin`. Agents assume `agent-task` is already available.

Use normal Git arguments for feature work:

```powershell
agent-task git add --all
agent-task git commit -m "Implement feature"
agent-task git rebase dev
agent-task git reset --hard dev
```

Arguments and streams pass directly to Git, with no shell or quoting reconstruction. Commits,
amends, resets, cleaning, merges, stashes and ordinary recovery are allowed on feature work.
Local `dev`, `main` and `master` may be revision inputs but cannot be mutated or attached to.
A worktree currently on a protected branch permits only simple read-only inspection.
Worktree destinations must be children of the current worktree root, including when run from
nested directories. Repository/environment redirection and aliases are rejected. Repository
administration, remote ref transfers and unmodeled arbitrary-ref interfaces require the maintainer.
Use `--no-update-refs` if rebase.updateRefs is enabled. Git owns locks and recovery state.
The cooperative no-bypass policy is in [AGENTS.md](../../AGENTS.md).

After validation and explicit user authorization, use `integrate-feature` from `dev.ps1`.
It queues the exclusive `integration/dev` jobserver lease and invokes `agent-task integrate`.
The privileged transaction requires clean feature/dev worktrees, rebases onto pinned dev,
runs cheap Git sanity checks, constructs a merge commit and advances dev with compare-and-swap.
It refreshes dev, returns persistent devN worktrees home and safely deletes the feature branch.
Use `integrate-feature -KeepBranch` to retain it. Worktrees without a devN home detach at the
integrated feature tip before deletion. A failed final rebase is aborted; resolve normally outside
the queue using `agent-task git`, validate and requeue. Promotion/cleanup failures report retained
state and require inspection before retrying. Integration does not orchestrate builds or tests.

Only `agent-task git` and ordinary preparation commands belong in unconditional allow rules;
never whitelist all of `agent-task`, since `integrate` is privileged.

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
