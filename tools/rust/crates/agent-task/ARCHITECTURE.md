# AgentTask architecture

AgentTask is installed outside `out`, allowing preparation to clear build output while the
executable runs. Commands resolve the current worktree and launch subprocesses directly.
Preparation and central-tool installation share recursive submodule synchronization/update.

Developer operations use explicit command dispatch. CMake retains build prerequisites and writes
`unreal-paths.json` with resolved editor, project, DDC, and staged-game paths. AgentTask consumes
those paths and requests existing prerequisite targets; it does not replicate their dependencies.
Standalone project generation resolves the two supported engine script layouts directly.
Formatting and tidy policies remain in the checkout. Tidy delegates workers to LLVM's runner,
and benchmark commands delegate to the revision-local benchmark-tools host executable.

## Git boundary

Typed Clap commands construct fresh Git arguments without a shell. Pathspecs and messages
retain literal values, and Git inherits terminal streams. Git owns locks and recovery state;
AgentTask provides cooperative workflow guardrails, not a hostile-process security boundary.

Local `dev`, `main`, `master`, and branches checked out by other registered worktrees may
be revision inputs but cannot be mutation or switch targets. Protected names ignore ASCII case;
ownership comparisons also ignore ASCII case on Windows. Ownership comes from
`git worktree list --porcelain -z`, with no separate registry. A protected current worktree
permits only status, branch listing, and worktree listing.

Managed worktrees live at `.local/worktrees/branch-<escaped-branch>` beneath the current root.
Lowercase ASCII letters, digits, hyphens, and underscores stay literal; other UTF-8 bytes use
`%xx` escapes. Removal requires the registered path to match that location. Symlink/junction
redirection is rejected. Creation requires the existing `.local/` ignore policy to prevent
a parent `add -A` from staging the child.

Rebases affect the current branch and disable updates to other refs. While HEAD is detached
during rebase, branch protection reads the original branch from the worktree's Git metadata.

## Integration transaction

The privileged integration command requires clean feature/dev worktrees and rebases onto
pinned dev with autostash, update-refs, and rebase-merges disabled. It runs cheap Git sanity
checks, constructs a merge commit, and advances dev with compare-and-swap.

It refreshes dev, returns persistent devN worktrees home, and deletes the feature branch unless
retention was requested. Worktrees without a devN home detach at the integrated feature tip
before deletion. A failed final rebase is aborted; promotion/cleanup failures report retained
state. Validation belongs to the caller and is not repeated during integration.

## Subprocesses and settings

The jobs adapter forwards commands to the installed jobserver without a shell. The jobs board
does not supervise or reap descendants; admission and cancellation belong to the outer workflow.

The Unreal adapter validates build-script/project paths, sets `IOJ_NATIVE_TOOLCHAIN`, and
forwards Unreal's exit code. Windows batch scripts use the command processor, with MSBuild
node reuse disabled for the child.

Live Coding edits preserve the saved INI file's encoding, byte-order mark, line endings,
whitespace, and comments. Missing settings are left absent; decoding or writing failures warn
without preventing code generation.

## Codex process ownership (Windows)

`codex start` creates a named Windows Job Object and assigns the launcher itself before
spawning `codex.exe --no-daemon` with the standard process API. Descendants inherit job
membership, including nested sandbox jobs. The name combines the worktree identity and
session name. Only the Codex PID is saved under `.local/codex/`.

The launcher waits in the existing console and lets Codex handle Ctrl+C. There is no daemon,
suspended launch, session recovery, or exit-time process cleanup. The cooperative jobs board
remains separate.

`clean` requires actual membership in the named job. It enumerates members and preserves
Codex installation executables, sandbox runners, console hosts, and the cleanup caller's
ancestry. Console hosts do not protect their worker ancestors. Held process handles and
membership checks scope termination to this job. Cleanup acts on one snapshot of child work.
Externally brokered processes that do not join the job are outside its scope.

## Tests

CLI integration tests live directly in `tests/`. Unit modules under `tests/<module>/mod.rs`
are included with `#[cfg(test)]` and `#[path]` so they retain private-module access without
exposing implementation APIs.
