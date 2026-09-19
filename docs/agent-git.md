# agent-git

`agent-git` is the constrained Git interface for autonomous coding agents. It is not an OS sandbox.
It is a policy engine,
not an argument wrapper: every supported operation is parsed into a semantic request, evaluated
against repository and worktree state, and either executed with fixed Git arguments or denied.
Unknown commands never fall through to Git.

The trusted path stays explicit: CLI parsing creates a closed semantic request; repository
discovery builds one state snapshot; the policy evaluator returns an allow/deny decision; and an
operation-specific executor constructs fixed Git arguments. Process launching and shared worktree
parsing are separate infrastructure. CLI strings never become Git subcommands or options.

## Trust model and installation

The trusted executable lives outside writable worktrees at:

```text
%LOCALAPPDATA%\NukeTheBees\agent-git\bin\agent-git.exe
```

After the implementation has been reviewed and merged to `dev`, load `dev.ps1` and run:

```powershell
install-agent-git -BaseBranch dev
```

The installer is intentionally separate from `agent-git` and must not be included in an
unconditional agent allow rule. A canonical install creates a random private build and staging
area for that invocation, builds AgentGit there, and runs the complete `AgentGit.Tests` security
regression suite against that build. It records SHA-256 hashes for every required runtime artifact
before the tests, rejects any post-validation change, copies only those private artifacts, and
verifies the staged and activated copies against the same hashes. It never returns to shared
`tools/bin` output after validation. The security gate and validation project cannot be skipped or
replaced for a canonical install; test-only controls are limited to temporary non-canonical
installs. A failed gate leaves the existing installation untouched; a failed activation or
post-install smoke check restores the previous installation.

The installer records the repository's canonical common Git directory,
origin URL, trusted Git and Git-LFS executables, user identity, and `refs/heads/dev` as the policy
authority. Runtime policy is read from `refs/heads/dev:.agent-git.json`; an edit in a feature
worktree cannot change active permissions before it is reviewed and merged.

Repository build output under `tools/bin` refuses repository operations. This prevents an agent
from editing the source, rebuilding a permissive binary, and using it through the trusted rule.
Installation activates the validated staged copy atomically and smoke-tests both the executable and
the registered repository trust path.

### Trust boundary

Internally, `agent-git` enforces the closed semantic command set, compiled safety ceilings,
repository identity and protected-ref policy authority, branch/worktree checks, direct structured
Git invocation, and controlled Git configuration and environment described below. Repository data
cannot select a Git subcommand or a program for Git to execute.

The outer agent permission sandbox must protect what this executable cannot: only the canonical
absolute executable path receives unconditional execution permission. Agents must not receive
unconditional permission to run the installer, invoke raw mutating Git, replace or modify the
canonical installation, trust manifest, or isolation configuration, change the permission rule, or
directly edit Git administrative data as a substitute for an unsupported operation. Deployment also
assumes a reviewed trusted source checkout that is not being modified concurrently while the
human-controlled installer runs, and trusted local .NET, Git, and optional Git-LFS executables.
These are deployment assumptions, not guarantees provided by `agent-git` itself.

## Policy

`.agent-git.json` is a strict, versioned document. Version 1 defines:

- `repositoryId` and `baseBranch`;
- exact or regular-expression matchers for `protected` and `workspace` branch groups;
- the optional semantic `lfs` Git extension;
- explicit policies for each supported mutation.

Branches matching neither configured group are features. A branch matching both groups is
ambiguous and fails closed. Unknown fields, operations, groups, extensions, or malformed patterns
invalidate the policy. Built-in safety ceilings prevent policy from enabling protected/workspace
commits, protected switching, arbitrary rebase targets, force deletion, or unsupported operations.
Every repository command, including inspection commands, requires a valid policy and trust
registration; broken configuration never creates a less-restricted mode.

In this repository, `dev`, `main`, and `master` are protected; branches matching `^dev[0-9]+$` are
persistent workspace branches; everything else is a feature branch. Workspace switching is limited
to the branch whose name matches the current worktree directory.

## Commands

Read-only inspection:

```text
agent-git status
agent-git branch-info
agent-git policy <operation> [target-branch]
```

Constrained mutations:

```text
agent-git add <paths...>
agent-git add-all
agent-git commit -m <message>
agent-git switch <branch>
agent-git switch-create <branch>
agent-git rebase-base
agent-git rebase-continue
agent-git rebase-abort
agent-git branch-delete <branch>
```

The repository's `integrate-feature` command is the sole protected-branch exception. After explicit
user authorization it acquires the jobserver's exclusive `integration/<baseBranch>` lease and then
invokes:

```text
agent-git integration-info [--json]
agent-git integrate --authorized [--keep-branch]
  [--maintainer-override --override-reason <reason>]
```

`agent-git` verifies that the visible lease ID is a running `integration` job for the current
worktree with the exact exclusive resource. It pins local `dev`, rebases once, prints review
material, and identifies the effective patch with a history-independent fingerprint. Review
receipts follow that fingerprint across history-only rebases and are invalidated by changed source
or conflict resolution.

The changed component/dependency surface selects the final gates. AgentGit, jobserver, C# tools,
scripts, and native code use focused gates without automatically scheduling Unreal. Unreal and
Development validation is reserved for Unreal-facing and cross-cutting build changes. Successful
receipts prevent rerunning an identical gate against the same candidate and environment.
Component ownership and dependency expansion come from `.integration-gates.json` at the pinned
base commit. The manifest selects only fixed built-in gate IDs; built-in minimum classifications
and conservative handling of unknown paths prevent a feature from exempting itself.

The final `dev` update is an atomic compare-and-swap from the pinned SHA to a no-fast-forward merge
commit. A mismatch reports both SHAs and aborts without retrying. Cleanup returns a `devN` worktree
to its home branch and uses safe branch deletion.

An explicit maintainer instruction can use
`integrate-feature -MaintainerOverride -OverrideReason '<reason>'`. The override never activates
implicitly: it lists skipped gates, requires candidate-specific confirmation, retains cheap Git
integrity checks, and records JSON audit data plus merge trailers.

Prefix any mutation with `--dry-run` to perform full discovery and policy evaluation without
executing a mutating Git command. Policy denial has a different exit code from invalid usage,
configuration failure, repository failure, and Git execution failure.

Staging and committing are limited to feature branches. Both switching commands may originate only
from workspace or feature branches, so an agent cannot move a protected integration worktree away
from its protected branch. Switching to a protected branch remains forbidden. Existing-branch
switching requires a fully clean worktree; switch-create may carry local changes onto a new feature
branch. Rebase-base uses only the configured local base branch and stops on conflicts. Branch
deletion remains available while on a protected branch, but requires a clean worktree, an unowned
feature branch, proven ancestry into the base, and Git's safe `branch -d` check.

### Rebase conflict recovery

If `rebase-base` stops at a conflict, resolve the files normally and use the existing staging
commands before continuing:

```text
agent-git rebase-base
# resolve files
agent-git add <resolved-path>...
# or: agent-git add-all
agent-git rebase-continue
```

Repeat the resolve, stage, and continue steps if another commit conflicts. To abandon the rebase,
use `agent-git rebase-abort`. During recovery, only `add`, `add-all`, `rebase-continue`, and
`rebase-abort` receive narrow exceptions to the normal operation-in-progress and detached-HEAD
rules. Commit, switch, switch-create, branch-delete, and another rebase-base remain denied.

Recovery is available only for a rebase started by AgentGit's constrained
`rebase-base` operation. Before starting Git, AgentGit records versioned provenance in the current
worktree's Git administrative directory. After a conflict, it binds that marker to Git's rebase
directory and validates the original feature branch and HEAD, exact base commit, protected policy
commit, repository identity, Git's rebase metadata, and the fixed pick plan. The marker is not a
policy authority: continuation still requires the current protected policy to classify the
original branch as a permitted feature branch and authorize rebasing. The live base branch may
advance independently; the active transaction remains bound to the exact recorded base commit in
Git's immutable `onto` metadata. If current policy semantics become restrictive, only abort remains
available; abort restores the recorded branch and pre-rebase HEAD.

An externally started rebase, malformed or mismatched provenance, changed rebase plan, or missing
binding is reported as unavailable and cannot use any recovery command or the staging exception.
A marker without an active matching rebase grants no authority; a later valid `rebase-base` safely
replaces that stale marker. Successful completion or abort removes the marker. Status reports
whether an active rebase has available, abort-only, or unavailable AgentGit recovery and, for an
owned rebase, identifies its original branch and recorded base.

## Intentionally unsupported

There is no raw/exec/passthrough command, repository/config/Git-path override, general merge, push,
fetch, reset, clean, restore, path checkout, arbitrary rebase target, force deletion, or force push.
The fixed integration transaction above is the only merge path and cannot select another target
branch or run without its matching jobserver lease. Unsupported mutations must use the normal
human-approval route. Raw read-only Git remains suitable
for inspection. Partial-clone/promisor repositories, custom LFS extensions, and redirected LFS
storage are also unsupported because they can introduce implicit remote processes or filesystem
writes outside the registered Git state.

Rebase skip is deliberately unsupported: dropping a commit is not part of the unconditional
recovery surface. AgentGit also never adopts or aborts a rebase started through raw Git.

The tool invokes a pinned Git executable directly with structured arguments and no shell. It uses
a controlled environment, explicitly enables Git's NTFS and HFS path protections, disables optional
locks for inspection, and disables hooks, signing, editors, pagers, prompts, automatic maintenance,
replace refs, update-refs rebasing, and inherited repository redirection. Mutation commands still
use Git's required integrity locks; `GIT_OPTIONAL_LOCKS=0` suppresses only opportunistic writes by
commands such as status. Unknown executable filters,
merge drivers, configuration includes, executable diff/merge/pager/GPG/submodule settings,
`core.worktree` redirection, hidden exclude files, fsmonitor commands, and object alternates fail
closed. Git LFS is the sole initially modeled external extension. Git output captured by the tool is
bounded so hostile repository state cannot cause unbounded diagnostic buffering.
LFS clean filtering remains available for staging, while automatic smudging is skipped so a branch
checkout cannot trigger repository-controlled network or credential activity; explicit LFS content
downloads remain outside the trusted interface.
On Git for Windows, Git starts the fixed `git-lfs filter-process` command through its bundled shell;
`agent-git` does not invoke a shell itself. The Git-LFS executable directory is pinned first in the
controlled `PATH`, the filter command names the trusted absolute executable, legacy fallback filter
commands are disabled, repository LFS filter commands are overridden, and all non-LFS filters are denied.

Mutations take a lock in the common Git directory and rediscover repository state under that lock
immediately before policy evaluation. Immediately before Git execution, the tool re-audits
executable configuration and hidden index flags, compares the exact index/worktree fingerprint,
revalidates HEAD, current branch, policy, base, and target refs, and confirms those branch refs are
still direct. Branch deletion also re-identifies its base worktree and verifies its repository,
branch, and HEAD before using it for Git's safe deletion check. The lock coordinates `agent-git`
processes; raw Git or other programs can still race the small interval after final validation, with
Git's own ref and index locks providing the final integrity checks.
The integration transaction takes this repository lock only around its rebase, atomic merge, and
cleanup mutations; it does not hold the lock during review or expensive validation, so other
feature work remains concurrent. The separate jobserver integration lease is held for the complete
transaction. Raw Git can still bypass that protocol, so the final compare-and-swap is authoritative
and exposes unexpected `dev` movement rather than retrying.
Worktree roots, mutation paths, and critical Git administrative paths containing filesystem
reparse points are rejected, as are assume-unchanged and skip-worktree index entries; these states
can hide changes or redirect I/O outside the registered worktree or common Git directory. Sparse
worktrees are therefore intentionally unsupported in v1.

## Codex rule

Whitelist the canonical absolute executable, not a worktree copy or an ambiguously resolved name.
Conceptually:

```text
prefix_rule(
    pattern=["%LOCALAPPDATA%\\NukeTheBees\\agent-git\\bin\\agent-git.exe"],
    decision="allow",
    justification="Repository-aware Git interface enforcing project policy.",
)
```

Mutating raw Git, `install-agent-git`, canonical trust/isolation file modification, and direct Git
administrative-data mutation must remain outside this unconditional allow path. The installer does
not edit Codex configuration.
