# AgentTask usage

## Installation

The maintainer installs or updates AgentTask from the repository root:

```powershell
pwsh -NoProfile -File PowerShell/InstallAgentTask.ps1
```

This runs package tests, installs AgentTask using the pinned Rust toolchain, and smoke-tests
the installed executable with `--version`. Installers create symlinks in
`%NTB_APPDATA_LOCAL%\bin`; add this directory to PATH. Windows Developer Mode or the
**Create symbolic links** privilege is required and checked before building.
Agents assume AgentTask is already available; they do not install it as a preflight step.

Pass `-SkipTests` to retry installation without repeating package tests that already passed:

```powershell
pwsh -NoProfile -File PowerShell/InstallAgentTask.ps1 -SkipTests
```

The install, executable `--version` check, and tool-link publication still run.

## Worktree preparation and central tools

Run `agent-task prepare-worktree` from anywhere inside the current worktree. It removes the
root `out` directory, synchronizes and initializes/updates recursive submodules, regenerates
CMake presets, disables Live Coding in existing saved Editor settings, and generates code.
It stops on the first failure, except a saved-settings failure warns and allows generation
to continue. Git, Python, CMake, and the repository build prerequisites must be available.
It does not perform a broad project/test build.

After preparation, the maintainer can run `agent-task install-central-tools`. This repeats
submodule synchronization/update, configures `native`, and builds the canonical
`install-jobserver` target, stopping on failure. It does not repeat the remaining preparation
steps. See [developer tools](../../../../README.md) for installation locations.

## Formatting, presets, and analysis

`agent-task format [--all|--changed|--staged] [--jobs N] [--verbose]` uses the worktree's
`.code-format.json` and `.clang-format`. The default selects all sources; changed selection
includes staged, unstaged, and untracked sources. Staged selection rejects files with unstaged
edits and re-stages only selected files after successful formatting. The default worker count
is half the logical processors, capped at 16. Diagnostics use repository-relative paths.

`agent-task presets [--check]` invokes the revision-local Python preset generator without
configuring CMake. Preparation uses the same operation.

Configure `win-x64-clangcl-debug-tidy`, then run:

```powershell
agent-task tidy --scope simulation --build-dir out/build/win-x64-clangcl-debug/clang-tidy
```

The default scope is `native`. Named scopes and exclusions live in `.clang-tidy-scopes.json`;
nested `.clang-tidy` policy still applies. `--jobs N` forwards LLVM's worker count (zero means
automatic). LLVM_ROOT/bin is exclusive when set; otherwise PATH supplies the tools. Full-native
and LispB analysis build the existing generated-input targets first. Logs stay in the analysis tree.

## Unreal operations and benchmarks

`agent-task unreal project-files` uses `UE_ROOT`, or `--ue-root <engine-root>`, without a CMake
configure. `--native-toolchain` defaults to the environment's `IOJ_NATIVE_TOOLCHAIN` or `clang-cl`.
Pass `--build-dir <configured-tree>` instead to reuse configured engine/toolchain settings.

`agent-task editor [--wait-for-debugger]` builds `editor` then launches with Live Coding disabled.
It defaults to `out/build/debug-game`; `--build-dir` selects another configured tree.
`agent-task run-staged` defaults to the configured Development tree and starts its staged executable
without building or packaging. Its working directory is the staged directory.

`agent-task unreal --help` lists explicit asset commands. They build `editor` and any additional
material compilation target before invoking existing commandlets. Mesh shapes use
`agent-task unreal generate-lab-mesh <shape>`. See the [asset commands](../../../../../docs/build-and-test.md#packaging-and-asset-maintenance).

`agent-task benchmark <operation> [options]` builds `benchmark-tools-host` in the native tree
and delegates all arguments to that revision-local tool. Workloads and reports remain in
benchmark-tools; see [benchmark operations](../../../../../docs/benchmarks.md).
These commands do not acquire jobs tickets; callers retain the existing coordination workflow.

CMake invokes `agent-task unreal-build --build-script <path> --target <target> --platform <platform>
--configuration <configuration> --project <path> --native-toolchain <name>` for Unreal targets.
Use the repository's [CMake workflows](../../../../../cmake/README.md) for builds.

## Feature Git operations

```powershell
agent-task git add -A
agent-task git commit -m "Implement feature"
agent-task git rebase dev
```

Supported commands are `status`, `add`, `commit`, `restore`, `reset`, `clean`, `rm`, `mv`,
`switch`, `branch`, `merge`, `rebase`, `cherry-pick`, `revert`, and `worktree`.
Run `agent-task git <command> --help` for exact options. Unknown commands, options, and abbreviated
long options are rejected. There is no checkout, remote transfer, config, plumbing, force-create,
explicit-branch rebase, or general worktree administration interface.
Cherry-pick and revert accept individual commits, not revision ranges.

Create managed worktrees with `agent-task git worktree add -b <branch> [start-point]`; remove
them with `agent-task git worktree remove <branch>`. AgentTask chooses a location under the
current worktree's ignored `.local/worktrees/`. Force removal is unsupported.

To park work, create a temporary feature branch and commit the WIP there; creating a branch
alone does not preserve dirty files. Git stash is unsupported because it is repository-global.
Use separately permitted read-only Git for inspection, such as `git diff` and `git log`.
Report unsupported mutations; raw mutations require an explicit maintainer exception.

## Named Codex sessions (Windows)

Launch from the worktree in an existing terminal:

```powershell
agent-task codex start dev1
```

The launcher joins a named Windows Job Object, then starts `codex --no-daemon` normally.
Codex and its later tool processes inherit membership. It uses the existing console and
does not open another PowerShell window. Keep running ordinary commands such as `ctest`
and `cmake` directly; their approval prefixes are unchanged.

Names contain 1-64 lowercase letters, digits, hyphens, or underscores and are scoped to
the current worktree. The launcher accepts no additional Codex arguments.

```powershell
agent-task codex processes dev1
agent-task codex clean dev1 --dry-run
agent-task codex clean dev1
```

Run `clean` inside that session. It stops the job's current child work, preserving Codex,
its runtime helpers, console hosts, and the cleanup command's ancestors. Processes outside
the job are untouched. Finish or pause other work first; cleanup can stop background tools
and third-party MCP servers in the session.

The launcher saves Codex's PID under ignored `.local/codex/` and waits for Codex to exit.
There is no crash recovery or automatic descendant termination on exit. Leftover processes
require manual cleanup; a job with remaining processes prevents reuse of its session name.
This tracks a launched CLI session, including its subagents, rather than an existing desktop
session or individual agents within a shared session.

Run `cargo test --locked -p agent-task --test codex_sessions` from `tools/rust` for the
focused launch, isolation, and cleanup tests.

## Integration and jobs

After validation and explicit user authorization, run `agent-task integrate` from the feature worktree.
Use `--keep-branch` to retain the feature branch. Integration does not build or test, and its
cheap Git transaction needs no jobs-board ticket. A final-rebase conflict is aborted; resolve
with `agent-task git rebase dev`, validate, and retry. Promotion or cleanup failures report
retained state and require inspection before retrying.

Use `agent-task jobs request|check|start|end|cancel|status` for manual shared/exclusive
coordination. Tickets remain until ended or cancelled. Follow the
[jobs-board workflow](../../../../jobserver/README.md).

`agent-task git`, `agent-task jobs`, ordinary preparation commands, and the narrowly scoped
`agent-task codex processes` and `agent-task codex clean` commands may have unconditional
allow rules. Keep `codex start` outside that allowance. Never whitelist all
of `agent-task`: `integrate` is privileged.
See the [repository policy](../../../../../AGENTS.md).
