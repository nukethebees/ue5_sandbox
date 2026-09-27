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

Use the intentionally limited Git interface for routine feature work:

```powershell
agent-task git add -A
agent-task git commit -m "Implement feature"
agent-task git rebase dev
agent-task git reset --hard dev
```

Supported commands are `status`, `add`, `commit`, `restore`, `reset`, `clean`, `rm`, `mv`,
`stash`, `switch`, `branch`, `merge`, `rebase`, `cherry-pick`, `revert`, and `worktree`.
Run `agent-task git <command> --help` for the exact options. Unknown commands, options and
abbreviated long options are rejected. There is no `checkout`, remote transfer, config, plumbing,
force-create, explicit-branch rebase, or general worktree administration interface. Stash operations
address the latest stash; cherry-pick/revert accept individual commits, not revision ranges.
Use separately permitted raw read-only Git for inspection such as `git diff` and `git log`.

Typed commands construct fresh Git arguments without a shell; pathspecs and messages retain their
literal values and Git inherits the terminal streams. Local `dev`, `main`, `master`, and branches
checked out by other registered worktrees may be revision inputs but cannot be mutation or switch
targets. Ownership comes from `git worktree list --porcelain -z`, with no separate registry.
A protected current worktree permits only status, branch listing, stash listing and worktree listing.
Worktree add/remove destinations must be children of the current worktree root, including when
run from nested directories. Creation requires `-b <feature-branch>`; removal never forces cleanup.
Rebases affect the current branch and explicitly disable updates to other refs. Git owns locks and
recovery state. This is a cooperative guardrail: report unsupported operations rather than bypassing
it, and add support only when a real workflow requires it. Raw mutations require an explicit
maintainer exception. See [AGENTS.md](../../AGENTS.md).

After validation and explicit user authorization, use `integrate-feature` from `dev.ps1`.
It queues the exclusive `integration/dev` jobserver lease and invokes `agent-task integrate`.
The privileged transaction requires clean feature/dev worktrees, rebases onto pinned dev with
autostash, update-refs and rebase-merges explicitly disabled, rechecks the integration lease,
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
