# Rust tools

This Cargo workspace holds the stable human/agent workflow CLI and revision-local domain tools.
Rustup selects the pinned toolchain from `rust-toolchain.toml` when commands run in this directory.

CMake builds `native-binary-tools` (mimalloc symbol generation/audit), `game-package-tools`
(package verification), and `benchmark-tools` (benchmark orchestration and reports) privately on demand.
They are not installed on PATH. Run focused tests with `cargo test --locked -p <package>` here,
or the matching CTest labels. These tools launch domain subprocesses directly; admission and
cancellation belong to the outer workflow. See [benchmarks](../../docs/benchmarks.md) for workloads.

## agent-task

Begin each new task from anywhere in its Git worktree with:

```powershell
agent-task prepare-worktree
```

This removes the worktree-root `out`, synchronizes and initializes/updates recursive submodules,
then regenerates CMake presets, disables Live Coding in existing saved Editor settings, and generates code. It does not perform a broad project/test build; build
only the targets relevant to the task afterward. It stops on the first failure and streams
subprocess output. Git, Python, CMake, and the repository build prerequisites must be available.

The maintainer installs/updates central per-user build tools explicitly when needed:

```powershell
agent-task install-central-tools
```

Run preparation first. This command synchronizes and initializes/updates recursive submodules,
configures `native`, and runs the canonical `install-jobserver` CMake target, stopping on failure.
It does not repeat the remaining worktree preparation steps.
Each tool lives in its own per-user bin directory; see
[developer tools](../README.md). This is a maintainer command, never an agent preflight.

The maintainer installs/updates `agent-task` itself from the repository root with:

```powershell
. .\dev.ps1
install-agent-task
```

This runs the package tests, installs only `agent-task` with the pinned Rust toolchain, and
smoke-tests the installed executable with `--version`. Installers create symlinks in
`%NTB_APPDATA_LOCAL%\bin`; add this single directory to PATH. Developer Mode or the
Windows **Create symbolic links** privilege is required and checked before building.
Agents assume `agent-task` is already available.

`agent-task format [--all|--changed|--staged] [--jobs N] [--verbose]` uses the current
worktree's `.code-format.json` and `.clang-format`. The default is all sources; changed selection
includes staged, unstaged, and untracked sources. Staged selection rejects files with unstaged
edits before formatting and re-stages only selected files after successful formatting, using the
same constrained Git checks as `agent-task git`. The default worker count is half the logical
processors, capped at 16. Diagnostics use repository-relative paths.

CMake invokes `agent-task unreal-build --build-script <path> --target <target> --platform <platform>
--configuration <configuration> --project <path> --native-toolchain <name>` for Unreal targets.
It validates both paths, sets `IOJ_NATIVE_TOOLCHAIN`, and forwards Unreal's exit code. Windows
batch scripts run through the command processor. MSBuild node reuse is disabled for that child:
the cooperative jobs board does not supervise or reap descendants.

Use the intentionally limited Git interface for routine feature work:

```powershell
agent-task git add -A
agent-task git commit -m "Implement feature"
agent-task git rebase dev
agent-task git reset --hard dev
```

Supported commands are `status`, `add`, `commit`, `restore`, `reset`, `clean`, `rm`, `mv`,
`switch`, `branch`, `merge`, `rebase`, `cherry-pick`, `revert`, and `worktree`.
Run `agent-task git <command> --help` for the exact options. Unknown commands, options and
abbreviated long options are rejected. There is no `checkout`, remote transfer, config, plumbing,
force-create, explicit-branch rebase, or general worktree administration interface.
Cherry-pick/revert accept individual commits, not revision ranges. Git stash is intentionally
unsupported because it is repository-global. To park work, create a temporary local feature branch
and commit the WIP there; creating a branch alone does not preserve dirty files. Do not use Git stash.
Use separately permitted raw read-only Git for inspection such as `git diff` and `git log`.

Typed commands construct fresh Git arguments without a shell; pathspecs and messages retain their
literal values and Git inherits the terminal streams. Local `dev`, `main`, `master`, and branches
checked out by other registered worktrees may be revision inputs but cannot be mutation or switch
targets. Protected names ignore ASCII case; ownership comparisons also ignore ASCII case on Windows.
Ownership comes from `git worktree list --porcelain -z`, with no separate registry.
A protected current worktree permits only status, branch listing and worktree listing.
Use `worktree add -b <branch> [start-point]` and `worktree remove <branch>`. AgentTask owns the path:
`.local/worktrees/branch-<escaped-branch>` beneath the current worktree root. Lowercase ASCII letters,
digits, hyphens and underscores stay literal; other UTF-8 bytes use `%xx` escapes. Removal checks the
registered path matches that location. Symlink/junction redirection is rejected, and creation requires
the existing `.local/` ignore policy so a parent `add -A` cannot stage the child. No force removal.
Rebases affect the current branch and explicitly disable updates to other refs. Git owns locks and
recovery state. This is a cooperative guardrail: report unsupported operations rather than bypassing
it, and add support only when a real workflow requires it. Raw mutations require an explicit
maintainer exception. See [AGENTS.md](../../AGENTS.md).

After validation and explicit user authorization, use `integrate-feature` from `dev.ps1`.
It calls `agent-task integrate` directly; this cheap Git transaction needs no jobs-board ticket.
The privileged transaction requires clean feature/dev worktrees, rebases onto pinned dev with
autostash, update-refs and rebase-merges explicitly disabled, runs cheap Git sanity checks,
constructs a merge commit and advances dev with compare-and-swap.
It refreshes dev, returns persistent devN worktrees home and safely deletes the feature branch.
Use `integrate-feature -KeepBranch` to retain it. Worktrees without a devN home detach at the
integrated feature tip before deletion. A failed final rebase is aborted; resolve normally outside
the integration command using `agent-task git`, validate and retry. Promotion/cleanup failures report retained
state and require inspection before retrying. Integration does not orchestrate builds or tests.

`agent-task jobs request|check|start|end|cancel|status` provides manual shared/exclusive coordination.
It forwards these operations to the installed jobserver CLI without a shell. Tickets are identified
only by ID and remain until explicitly ended/cancelled. See the [jobs-board workflow](../jobserver/README.md).

Only `agent-task git`, `agent-task jobs`, and ordinary preparation commands belong in unconditional allow rules;
never whitelist all of `agent-task`, since `integrate` is privileged.

The executable lives outside `out`, so it can clear build output while running. For development,
build and test directly from `tools/rust`; these tests are not part of broad CTest bundles:

```powershell
cargo build --release --package agent-task --locked
cargo test --package agent-task --locked
```

The CLI tests use temporary Git repositories and small Python/CMake fixtures; they do not build
the game or clear this repository's output.
