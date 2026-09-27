use crate::git_cli::{Cli, Operation, Stash, Worktree};
use crate::workspace::{self, query};
use clap::Parser;
use std::ffi::OsString;
use std::path::Path;
use std::process::Command;

fn revision(root: &Path, value: &str, kind: &str) -> Result<OsString, String> {
    query(
        root,
        &[
            "rev-parse",
            "--verify",
            "--end-of-options",
            &format!("{value}^{{{kind}}}"),
        ],
    )
    .map(Into::into)
}

fn branch_name(root: &Path, name: &str, worktrees: &[workspace::Worktree]) -> Result<(), String> {
    // Full ref validation does not expand checkout-history expressions such as @{-1}.
    if name.starts_with('-') {
        return Err(
            "Branch names must not start with '-'. Use a local feature branch name.".into(),
        );
    }
    query(root, &["check-ref-format", &format!("refs/heads/{name}")])?;
    workspace::check_branch_target(name, root, worktrees)
}

impl Operation {
    fn read_only(&self) -> bool {
        matches!(
            self,
            Self::Status { .. }
                | Self::Stash {
                    action: Stash::List
                }
                | Self::Worktree {
                    action: Worktree::List
                }
        ) || matches!(self, Self::Branch(b) if b.name.is_none() && b.delete.is_none() && b.force_delete.is_none() && b.rename.is_none())
    }

    fn arguments(self, root: &Path, cwd: &Path) -> Result<Vec<OsString>, String> {
        let worktrees = workspace::worktrees(root)?;
        if !self.read_only() {
            let mut current = workspace::branch(root)?;
            // During rebase HEAD is detached, but recovery still updates the original branch.
            if current.is_empty() {
                let admin = query(root, &["rev-parse", "--absolute-git-dir"])?;
                for state in ["rebase-merge/head-name", "rebase-apply/head-name"] {
                    let path = Path::new(&admin).join(state);
                    if path.exists() {
                        let name = std::fs::read_to_string(path).map_err(|e| e.to_string())?;
                        current = name
                            .trim()
                            .strip_prefix("refs/heads/")
                            .unwrap_or("")
                            .to_owned();
                        break;
                    }
                }
            }
            workspace::check_branch_target(&current, root, &worktrees)?;
        }
        let mut args: Vec<OsString> = Vec::new();
        // Only these constructed arguments reach Git; the caller's argv is never forwarded.
        let mut push = |s: &str| args.push(s.into());
        match self {
            Self::Status { short } => {
                push("status");
                if short {
                    push("--short");
                }
            }
            Self::Add(a) => {
                push("add");
                if a.all {
                    push("-A");
                }
                if a.update {
                    push("-u");
                }
                if a.patch {
                    push("-p");
                }
                push("--");
                args.extend(a.paths);
            }
            Self::Commit(c) => {
                push("commit");
                if c.amend {
                    push("--amend");
                }
                if c.no_edit {
                    push("--no-edit");
                }
                if c.allow_empty {
                    push("--allow-empty");
                }
                if let Some(message) = c.message {
                    push("-m");
                    args.push(message);
                }
            }
            Self::Restore {
                staged,
                source,
                paths,
            } => {
                push("restore");
                if staged {
                    push("--staged");
                }
                if let Some(source) = source {
                    push("--source");
                    args.push(revision(root, &source, "tree")?);
                }
                args.push("--".into());
                args.extend(paths);
            }
            Self::Reset(r) => {
                push("reset");
                push(if r.soft {
                    "--soft"
                } else if r.hard {
                    "--hard"
                } else {
                    "--mixed"
                });
                args.push(revision(
                    root,
                    r.revision.as_deref().unwrap_or("HEAD"),
                    "commit",
                )?);
                args.push("--".into());
            }
            Self::Clean(c) => {
                push("clean");
                push(if c.dry_run { "-n" } else { "-f" });
                if c.directories {
                    push("-d");
                }
            }
            Self::Rm {
                recursive,
                cached,
                paths,
            } => {
                push("rm");
                if recursive {
                    push("-r");
                }
                if cached {
                    push("--cached");
                }
                push("--");
                args.extend(paths);
            }
            Self::Mv {
                source,
                destination,
            } => {
                workspace::contained(root, cwd, &source)?;
                workspace::contained(root, cwd, &destination)?;
                push("mv");
                push("--");
                args.extend([source, destination]);
            }
            Self::Stash { action } => {
                push("stash");
                match action {
                    Stash::Push { message } => {
                        push("push");
                        if let Some(message) = message {
                            push("-m");
                            args.push(message);
                        }
                    }
                    Stash::Pop => push("pop"),
                    Stash::Apply => push("apply"),
                    Stash::List => push("list"),
                    Stash::Drop => push("drop"),
                }
            }
            Self::Switch(s) => {
                push("switch");
                if let Some(name) = s.create {
                    branch_name(root, &name, &worktrees)?;
                    push("--no-track");
                    push("-c");
                    push(&name);
                    if let Some(start) = s.target {
                        args.push(revision(root, &start, "commit")?);
                    }
                } else if s.detach {
                    push("--detach");
                    args.push(revision(
                        root,
                        s.target.as_deref().unwrap_or("HEAD"),
                        "commit",
                    )?);
                } else {
                    let mut name = s.target.expect("clap requires a branch");
                    if name == "-" {
                        let previous =
                            query(root, &["rev-parse", "--symbolic-full-name", "@{-1}"])?;
                        name = previous.strip_prefix("refs/heads/").ok_or("Previous checkout is not a local feature branch. Use switch --detach <revision>.")?.to_owned();
                    }
                    branch_name(root, &name, &worktrees)?;
                    query(
                        root,
                        &["show-ref", "--verify", &format!("refs/heads/{name}")],
                    )?;
                    push("--no-guess");
                    push("--");
                    push(&name);
                }
            }
            Self::Branch(b) => {
                push("branch");
                if let Some(name) = b.delete.or(b.force_delete.clone()) {
                    branch_name(root, &name, &worktrees)?;
                    push(if b.force_delete.is_some() { "-D" } else { "-d" });
                    push("--");
                    push(&name);
                } else if let Some(name) = b.rename {
                    branch_name(root, &name, &worktrees)?;
                    push("-m");
                    push("--");
                    push(&name);
                } else if let Some(name) = b.name {
                    branch_name(root, &name, &worktrees)?;
                    push("--no-track");
                    push("--");
                    push(&name);
                    if let Some(start) = b.start {
                        args.push(revision(root, &start, "commit")?);
                    }
                } else {
                    push("--list");
                }
            }
            Self::Merge(m) => {
                push("merge");
                if m.resume {
                    push("--continue");
                } else if m.abort {
                    push("--abort");
                } else {
                    if m.no_edit {
                        push("--no-edit");
                    }
                    args.push(revision(root, m.revision.as_deref().unwrap(), "commit")?);
                }
            }
            Self::Rebase(r) => {
                push("rebase");
                if r.resume {
                    push("--continue");
                } else if r.abort {
                    push("--abort");
                } else if r.skip {
                    push("--skip");
                } else {
                    push("--no-update-refs");
                    if let Some(onto) = r.onto {
                        push("--onto");
                        args.push(revision(root, &onto, "commit")?);
                    }
                    args.push(revision(root, r.upstream.as_deref().unwrap(), "commit")?);
                }
            }
            Self::CherryPick(c) => {
                push("cherry-pick");
                if c.resume {
                    push("--continue");
                } else if c.abort {
                    push("--abort");
                } else if c.skip {
                    push("--skip");
                } else {
                    for commit in c.commits {
                        args.push(revision(root, &commit, "commit")?);
                    }
                }
            }
            Self::Revert(r) => {
                push("revert");
                if r.resume {
                    push("--continue");
                } else if r.abort {
                    push("--abort");
                } else {
                    if r.no_edit {
                        push("--no-edit");
                    }
                    for commit in r.commits {
                        args.push(revision(root, &commit, "commit")?);
                    }
                }
            }
            Self::Worktree { action } => {
                push("worktree");
                match action {
                    Worktree::List => push("list"),
                    Worktree::Add {
                        branch,
                        path,
                        start,
                    } => {
                        branch_name(root, &branch, &worktrees)?;
                        workspace::contained(root, cwd, &path)?;
                        push("add");
                        push("--no-track");
                        push("-b");
                        push(&branch);
                        push("--");
                        args.push(cwd.join(path).into_os_string());
                        args.push(revision(
                            root,
                            start.as_deref().unwrap_or("HEAD"),
                            "commit",
                        )?);
                    }
                    Worktree::Remove { path } => {
                        workspace::contained(root, cwd, &path)?;
                        let destination = cwd.join(path);
                        let target = workspace::resolved(&destination)?;
                        if target == workspace::resolved(root)? {
                            return Err("Cannot remove the current workspace. Ask the maintainer to retire it.".into());
                        }
                        if worktrees.iter().any(|w| {
                            w.path == target
                                && w.branch.as_deref().is_some_and(workspace::protected)
                        }) {
                            return Err("Cannot remove a protected worktree. Ask the maintainer to manage it.".into());
                        }
                        push("remove");
                        push("--");
                        args.push(destination.into_os_string());
                    }
                }
            }
        }
        Ok(args)
    }
}

pub fn run(arguments: &[OsString]) -> Result<i32, String> {
    let cli = match Cli::try_parse_from(
        std::iter::once(OsString::from("agent-task git")).chain(arguments.iter().cloned()),
    ) {
        Ok(cli) => cli,
        Err(error) => {
            error.print().map_err(|e| e.to_string())?;
            return Ok(error.exit_code());
        }
    };
    workspace::check_environment()?;
    let cwd = std::env::current_dir().map_err(|e| e.to_string())?;
    let root = workspace::root(&cwd)?;
    let args = cli.operation.arguments(&root, &cwd)?;
    Command::new("git")
        .args(args)
        .status()
        .map(|s| s.code().unwrap_or(1))
        .map_err(|e| format!("Could not run Git: {e}"))
}
