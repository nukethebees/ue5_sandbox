# agent-git

`agent-git` is the constrained Git interface for autonomous coding agents. It is a policy engine,
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
unconditional agent allow rule. It records the repository's canonical common Git directory,
origin URL, trusted Git and Git-LFS executables, user identity, and `refs/heads/dev` as the policy
authority. Runtime policy is read from `refs/heads/dev:.agent-git.json`; an edit in a feature
worktree cannot change active permissions before it is reviewed and merged.

Repository build output under `tools/bin` refuses repository operations. This prevents an agent
from editing the source, rebuilding a permissive binary, and using it through the trusted rule.

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
agent-git branch-delete <branch>
```

Prefix any mutation with `--dry-run` to perform full discovery and policy evaluation without
executing a mutating Git command. Policy denial has a different exit code from invalid usage,
configuration failure, repository failure, and Git execution failure.

Staging and committing are limited to feature branches. Existing-branch switching requires a
fully clean worktree; switch-create may carry local changes onto a new feature branch. Rebase-base
uses only the configured local base branch and stops on conflicts. Branch deletion requires a clean
worktree, an unowned feature branch, proven ancestry into the base, and Git's safe `branch -d` check.

## Intentionally unsupported

There is no raw/exec/passthrough command, repository/config/Git-path override, merge, push, fetch,
reset, clean, restore, path checkout, arbitrary rebase target, force deletion, or force push.
Unsupported mutations must use the normal human-approval route. Raw read-only Git remains suitable
for inspection.

The tool invokes a pinned Git executable directly with structured arguments and no shell. It uses
a controlled environment, disables hooks, signing, editors, pagers, prompts, automatic maintenance,
replace refs, update-refs rebasing, and inherited repository redirection. Unknown executable filters,
merge drivers, configuration includes, `core.worktree` redirection, and object alternates fail closed. Git LFS is the sole
initially modeled external extension.
LFS clean filtering remains available for staging, while automatic smudging is skipped so a branch
checkout cannot trigger repository-controlled network or credential activity; explicit LFS content
downloads remain outside the trusted interface.

Mutations take a lock in the common Git directory and rediscover repository state under that lock
immediately before policy evaluation. The lock coordinates `agent-git` processes; raw Git or other
programs can still race it, with Git's own ref and index locks providing the final integrity checks.

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

Mutating raw Git and `install-agent-git` must remain outside this unconditional allow path. The
installer does not edit Codex configuration.
