# coj usage

## Installation

The maintainer installs or updates coj from the repository root:

```powershell
pwsh -NoProfile -File PowerShell/InstallCoj.ps1
```

This runs package tests, installs coj using the pinned Rust toolchain, and smoke-tests
the installed executable with `--version`. Installers create symlinks in
`%IOJ_ROOT%\tools\bin`; set `IOJ_ROOT` and add this directory to PATH. Windows Developer Mode or the
**Create symbolic links** privilege is required and checked before building.
Agents assume coj is already available; they do not install it as a preflight step.

Pass `-SkipTests` to retry installation without repeating package tests that already passed:

```powershell
pwsh -NoProfile -File PowerShell/InstallCoj.ps1 -SkipTests
```

The install, executable `--version` check, and tool-link publication still run.

The installer stages the update before renaming the previous executable, allowing existing
`coj codex start` sessions to continue. It prints the rename and retained-file status at the
end; subsequent installs remove retired copies once they are no longer in use.

## Worktree preparation and central tools

Run `coj prepare-worktree` from anywhere inside the current worktree. It removes the
root `out` directory, synchronizes and initializes/updates recursive submodules, regenerates
CMake presets, disables Live Coding in existing saved Editor settings, and generates code.
It stops on the first failure, except a saved-settings failure warns and allows generation
to continue. Git, Python, CMake, and the repository build prerequisites must be available.
It does not perform a broad project/test build.

After preparation, the maintainer can run `coj install central-tools`. This repeats
submodule synchronization/update, configures `native`, and builds the C++ `jobserverd`
target through CMake. Rust then replaces the installed daemon, registers its Windows logon
task, and checks readiness. It stops on failure and does not repeat the remaining preparation
steps. See [developer tools](../../../../README.md) for installation locations.

Use `coj install jobserver` to select only jobserver. Both install commands accept `--force`
to close active board tickets before replacement and report what was closed. Ticket processes
keep running. See [jobserver installation](../../../../jobserver/docs/server.md).

## Formatting, presets, and analysis

`coj format [--all|--changed|--staged] [--jobs N] [--verbose]` uses the worktree's
`.code-format.json` and `.clang-format`. The default selects all sources; changed selection
includes staged, unstaged, and untracked sources. Staged selection rejects files with unstaged
edits and re-stages only selected files after successful formatting. The default worker count
is half the logical processors, capped at 16. Diagnostics use repository-relative paths.

`coj presets [--check]` invokes the revision-local Python preset generator without
configuring CMake. Preparation uses the same operation.

Configure `win-x64-clangcl-debug-tidy`, then run:

```powershell
coj tidy --scope simulation --build-dir out/build/win-x64-clangcl-debug/clang-tidy
```

The default scope is `native`. Named scopes and exclusions live in `.clang-tidy-scopes.json`;
nested `.clang-tidy` policy still applies. `--jobs N` forwards LLVM's worker count (zero means
automatic). LLVM_ROOT/bin is exclusive when set; otherwise PATH supplies the tools. Full-native
and LispB analysis build the existing generated-input targets first. Logs stay in the analysis tree.

## Unreal operations and benchmarks

`coj unreal project-files` uses `UE_ROOT`, or `--ue-root <engine-root>`, without a CMake
configure. `--native-toolchain` defaults to the environment's `IOJ_NATIVE_TOOLCHAIN` or `clang-cl`.
Pass `--build-dir <configured-tree>` instead to reuse configured engine/toolchain settings.

`coj editor [--wait-for-debugger]` builds `editor` then launches with Live Coding disabled.
It defaults to `out/build/debug-game`; `--build-dir` selects another configured tree.
`coj run-staged` defaults to the configured Development tree and starts its staged executable
without building or packaging. Its working directory is the staged directory.

`coj unreal --help` lists explicit asset commands. They build `editor` and any additional
material compilation target before invoking existing commandlets. Mesh shapes use
`coj unreal generate-lab-mesh <shape>`. See the [asset commands](../../../../../docs/build-and-test.md#packaging-and-asset-maintenance).

`coj benchmark <operation> [options]` builds `benchmark-tools-host` in the native tree
and delegates all arguments to that revision-local tool. Workloads and reports remain in
benchmark-tools; see [benchmark operations](../../../../../docs/benchmarks.md).
These commands do not acquire jobs tickets; callers retain the existing coordination workflow.

CMake invokes `coj unreal-build --build-script <path> --target <target> --platform <platform>
--configuration <configuration> --project <path> --native-toolchain <name>` for Unreal targets.
Use the repository's [CMake workflows](../../../../../cmake/README.md) for builds.

## Feature Git operations

```powershell
coj git add -A
coj git commit -m "Implement feature"
coj git rebase dev
```

Supported commands are `status`, `add`, `commit`, `restore`, `reset`, `clean`, `rm`, `mv`,
`switch`, `branch`, `merge`, `rebase`, `cherry-pick`, `revert`, and `worktree`.
Run `coj git <command> --help` for exact options. Unknown commands, options, and abbreviated
long options are rejected. There is no checkout, remote transfer, config, plumbing, force-create,
explicit-branch rebase, or general worktree administration interface.
Cherry-pick and revert accept individual commits, not revision ranges.

Create managed worktrees with `coj git worktree add -b <branch> [start-point]`; remove
them with `coj git worktree remove <branch>`. coj chooses a location under the
current worktree's ignored `.local/worktrees/`. Force removal is unsupported.

To park work, create a temporary feature branch and commit the WIP there; creating a branch
alone does not preserve dirty files. Git stash is unsupported because it is repository-global.
Use separately permitted read-only Git for inspection, such as `git diff` and `git log`.
Report unsupported mutations; raw mutations require an explicit maintainer exception.

## Named Codex sessions (Windows)

Launch from the worktree in an existing terminal:

```powershell
coj codex start dev1
```

The launcher joins a named Windows Job Object, then starts `codex --no-daemon` normally.
Codex and its later tool processes inherit membership. It uses the existing console and
does not open another PowerShell window. Keep running ordinary commands such as `ctest`
and `cmake` directly; their approval prefixes are unchanged.
The launcher sets `TMP` and `TEMP` to `%IOJ_ROOT%\tmp\<worktree-name>`, creating it if needed.
It reads `IOJ_ROOT` from the environment and derives the worktree name from the checkout it starts in.
Direct Cargo commands and other child tools inherit it. Allow writes there in the sandbox.

Names contain 1-64 lowercase letters, digits, hyphens, or underscores and are scoped to
the current worktree. The launcher accepts no additional Codex arguments.

```powershell
coj codex processes dev1
coj codex clean dev1 --dry-run
coj codex clean dev1
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

After building the `tool-tests` preset, run `ctest --test-dir out/build/native -L '^coj$'`
for coj tests with the configured temporary directory. For a focused direct run,
`cargo test --locked -p coj --test codex_sessions` from `tools/rust` reads `IOJ_ROOT`
through the shared test-support crate; no `TMP`/`TEMP` override is needed.

## Integration and jobs

After validation and explicit user authorization, run `coj integrate` from the feature worktree.
Use `--keep-branch` to retain the feature branch. Integration does not build or test, and its
cheap Git transaction needs no jobs-board ticket. A final-rebase conflict is aborted; resolve
with `coj git rebase dev`, validate, and retry. Promotion or cleanup failures report
retained state and require inspection before retrying.

Use `coj jobs` for manual shared/exclusive coordination. Tickets carry the current session
name and worktree, and remain until ended, cancelled, or explicitly cleared. Maintainers can
clear one ID or an owner with `coj jobs clear <id>` or `coj jobs clear --owner <name>`;
add `--worktree <path>` to limit owner clearing. No board command affects processes. Follow the
[jobs-board workflow](../../../../jobserver/README.md).

`coj git`, `coj jobs`, ordinary preparation commands, and the narrowly scoped
`coj codex processes` and `coj codex clean` commands may have unconditional
allow rules. Keep `codex start` outside that allowance. Never whitelist all
of `coj`: `integrate` is privileged.
See the [repository policy](../../../../../AGENTS.md).
