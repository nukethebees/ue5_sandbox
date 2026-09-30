use crate::git_cli::{Cli, Operation, Worktree};
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

fn push_flags(args: &mut Vec<OsString>, flags: &[(bool, &str)]) {
    args.extend(
        flags
            .iter()
            .filter(|(enabled, _)| *enabled)
            .map(|(_, flag)| OsString::from(flag)),
    );
}

fn push_operands(args: &mut Vec<OsString>, operands: impl IntoIterator<Item = OsString>) {
    args.push("--".into());
    args.extend(operands);
}

impl Operation {
    fn read_only(&self) -> bool {
        matches!(
            self,
            Self::Status { .. }
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
                let git_dir = query(root, &["rev-parse", "--absolute-git-dir"])?;
                for state in ["rebase-merge/head-name", "rebase-apply/head-name"] {
                    let path = Path::new(&git_dir).join(state);
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

        match self {
            Self::Status { short } => {
                args.push("status".into());
                push_flags(&mut args, &[(short, "--short")]);
            }
            Self::Add(a) => {
                args.push("add".into());
                push_flags(
                    &mut args,
                    &[(a.all, "-A"), (a.update, "-u"), (a.patch, "-p")],
                );

                push_operands(&mut args, a.paths);
            }
            Self::Commit(c) => {
                args.push("commit".into());
                push_flags(
                    &mut args,
                    &[
                        (c.amend, "--amend"),
                        (c.no_edit, "--no-edit"),
                        (c.allow_empty, "--allow-empty"),
                    ],
                );
                if let Some(message) = c.message {
                    args.push("-m".into());
                    args.push(message);
                }
            }
            Self::Restore {
                staged,
                source,
                paths,
            } => {
                args.push("restore".into());
                push_flags(&mut args, &[(staged, "--staged")]);
                if let Some(source) = source {
                    args.push("--source".into());
                    args.push(revision(root, &source, "tree")?);
                }

                push_operands(&mut args, paths);
            }
            Self::Reset(r) => {
                args.push("reset".into());
                args.push(
                    (if r.soft {
                        "--soft"
                    } else if r.hard {
                        "--hard"
                    } else {
                        "--mixed"
                    })
                    .into(),
                );
                args.push(revision(
                    root,
                    r.revision.as_deref().unwrap_or("HEAD"),
                    "commit",
                )?);

                args.push("--".into());
            }
            Self::Clean(c) => {
                args.push("clean".into());
                args.push((if c.dry_run { "-n" } else { "-f" }).into());
                push_flags(&mut args, &[(c.directories, "-d")]);
            }
            Self::Rm {
                recursive,
                cached,
                paths,
            } => {
                args.push("rm".into());
                push_flags(&mut args, &[(recursive, "-r"), (cached, "--cached")]);

                push_operands(&mut args, paths);
            }
            Self::Mv {
                source,
                destination,
            } => {
                workspace::contained(root, cwd, &source)?;
                workspace::contained(root, cwd, &destination)?;
                args.push("mv".into());

                push_operands(&mut args, [source, destination]);
            }
            Self::Switch(s) => {
                args.push("switch".into());
                if let Some(name) = s.create {
                    branch_name(root, &name, &worktrees)?;
                    args.push("--no-track".into());
                    args.push("-c".into());
                    args.push(name.into());
                    if let Some(start) = s.target {
                        args.push(revision(root, &start, "commit")?);
                    }
                } else if s.detach {
                    args.push("--detach".into());
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

                    args.push("--no-guess".into());

                    push_operands(&mut args, [name.into()]);
                }
            }
            Self::Branch(b) => {
                args.push("branch".into());
                if let Some(name) = b.delete.or(b.force_delete.clone()) {
                    branch_name(root, &name, &worktrees)?;
                    args.push((if b.force_delete.is_some() { "-D" } else { "-d" }).into());

                    push_operands(&mut args, [name.into()]);
                } else if let Some(name) = b.rename {
                    branch_name(root, &name, &worktrees)?;
                    args.push("-m".into());

                    push_operands(&mut args, [name.into()]);
                } else if let Some(name) = b.name {
                    branch_name(root, &name, &worktrees)?;
                    args.push("--no-track".into());

                    push_operands(&mut args, [name.into()]);
                    if let Some(start) = b.start {
                        args.push(revision(root, &start, "commit")?);
                    }
                } else {
                    args.push("--list".into());
                }
            }
            Self::Merge(m) => {
                args.push("merge".into());
                push_flags(&mut args, &[(m.resume, "--continue"), (m.abort, "--abort")]);
                if !m.resume && !m.abort {
                    push_flags(&mut args, &[(m.no_edit, "--no-edit")]);
                    args.push(revision(root, m.revision.as_deref().unwrap(), "commit")?);
                }
            }
            Self::Rebase(r) => {
                args.push("rebase".into());
                push_flags(
                    &mut args,
                    &[
                        (r.resume, "--continue"),
                        (r.abort, "--abort"),
                        (r.skip, "--skip"),
                    ],
                );
                if !r.resume && !r.abort && !r.skip {
                    args.push("--no-update-refs".into());
                    if let Some(onto) = r.onto {
                        args.push("--onto".into());
                        args.push(revision(root, &onto, "commit")?);
                    }
                    args.push(revision(root, r.upstream.as_deref().unwrap(), "commit")?);
                }
            }
            Self::CherryPick(c) => {
                args.push("cherry-pick".into());
                push_flags(
                    &mut args,
                    &[
                        (c.resume, "--continue"),
                        (c.abort, "--abort"),
                        (c.skip, "--skip"),
                    ],
                );
                if !c.resume && !c.abort && !c.skip {
                    for commit in c.commits {
                        args.push(revision(root, &commit, "commit")?);
                    }
                }
            }
            Self::Revert(r) => {
                args.push("revert".into());
                push_flags(&mut args, &[(r.resume, "--continue"), (r.abort, "--abort")]);
                if !r.resume && !r.abort {
                    push_flags(&mut args, &[(r.no_edit, "--no-edit")]);
                    for commit in r.commits {
                        args.push(revision(root, &commit, "commit")?);
                    }
                }
            }
            Self::Worktree { action } => {
                args.push("worktree".into());
                match action {
                    Worktree::List => args.push("list".into()),
                    Worktree::Add { branch, start } => {
                        branch_name(root, &branch, &worktrees)?;
                        let path = workspace::managed_worktree_path(root, &branch)?;
                        let ignored = Command::new("git")
                            .args(["check-ignore", "--quiet", "--"])
                            .arg(&path)
                            .current_dir(root)
                            .status()
                            .map_err(|e| e.to_string())?;
                        if !ignored.success() {
                            return Err("The managed worktree destination must be ignored by the repository's .local/ policy before creating a worktree. Ask the maintainer to restore that policy.".into());
                        }

                        args.push("add".into());
                        args.push("--no-track".into());
                        args.push("-b".into());
                        args.push(branch.into());

                        push_operands(&mut args, [path.into_os_string()]);
                        args.push(revision(
                            root,
                            start.as_deref().unwrap_or("HEAD"),
                            "commit",
                        )?);
                    }
                    Worktree::Remove { branch } => {
                        if workspace::protected(&branch) {
                            return Err("Cannot remove a protected worktree. Ask the maintainer to manage it.".into());
                        }
                        let registered = worktrees.iter().find(|w| w.branch.as_deref()
                            .is_some_and(|b| workspace::same_branch(b, &branch)))
                            .ok_or("No registered worktree for that branch. Use agent-task git worktree list.")?;
                        let destination = workspace::managed_worktree_path(
                            root,
                            registered.branch.as_deref().unwrap(),
                        )?;
                        if registered.path != workspace::resolved(&destination)? {
                            return Err("Removal is limited to AgentTask-owned worktrees beneath this workspace's .local/worktrees/. Ask the maintainer to manage other worktrees.".into());
                        }

                        args.push("remove".into());

                        push_operands(&mut args, [destination.into_os_string()]);
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
