use clap::{Args, Parser, Subcommand};
use std::ffi::OsString;

#[derive(Parser)]
#[command(
    name = "agent-task git",
    about = "Run supported Git operations within feature-workspace guardrails. Intentionally supports only common feature-development operations."
)]
pub struct Cli {
    #[command(subcommand)]
    pub operation: Operation,
}

#[derive(Subcommand)]
pub enum Operation {
    /// Inspect working tree status.
    Status {
        #[arg(long)]
        short: bool,
    },
    /// Stage paths, all changes, tracked changes, or interactive patches.
    Add(Add),
    /// Commit staged changes or amend the current commit.
    Commit(Commit),
    /// Restore paths in the working tree or index.
    Restore {
        #[arg(long)]
        staged: bool,
        #[arg(long)]
        source: Option<String>,
        #[arg(required = true)]
        paths: Vec<OsString>,
    },
    /// Reset only the current feature branch/worktree (default: mixed HEAD).
    Reset(Reset),
    /// Preview untracked files (-n), or remove them (-f, optionally -d).
    Clean(Clean),
    /// Remove paths from the index and optionally the working tree.
    Rm {
        #[arg(short = 'r')]
        recursive: bool,
        #[arg(long)]
        cached: bool,
        #[arg(required = true)]
        paths: Vec<OsString>,
    },
    /// Move a file or directory within this workspace.
    Mv {
        source: OsString,
        destination: OsString,
    },
    /// Switch to a local feature branch, create one, or detach. '-' means previous branch.
    Switch(Switch),
    /// List/create branches, delete an unowned feature branch, or rename the current branch.
    Branch(Branch),
    /// Merge one revision into the current feature branch, or recover a merge.
    Merge(Merge),
    /// Rebase the current branch only; never update other refs.
    Rebase(Rebase),
    /// Apply individual commits to the current feature branch, or recover.
    CherryPick(CherryPick),
    /// Revert individual commits on the current feature branch, or recover.
    Revert(Revert),
    /// List worktrees, or create/remove branch-named worktrees under .local/worktrees/.
    Worktree {
        #[command(subcommand)]
        action: Worktree,
    },
}

#[derive(Args)]
#[group(skip)]
pub struct Add {
    #[arg(short = 'A', group = "stage_mode")]
    pub all: bool,
    #[arg(short = 'u', group = "stage_mode")]
    pub update: bool,
    #[arg(short = 'p', group = "stage_mode")]
    pub patch: bool,
    #[arg(required_unless_present_any = ["all", "update", "patch"])]
    pub paths: Vec<OsString>,
}

#[derive(Args)]
#[group(skip)]
#[command(
    after_help = "Forms: commit -m <message> | commit --amend [-m <message> | --no-edit]\n       commit --allow-empty -m <message>"
)]
pub struct Commit {
    #[arg(
        short = 'm',
        required_unless_present = "amend",
        allow_hyphen_values = true
    )]
    pub message: Option<OsString>,
    #[arg(long)]
    pub amend: bool,
    #[arg(long, requires = "amend", conflicts_with = "message")]
    pub no_edit: bool,
    #[arg(long, requires = "message")]
    pub allow_empty: bool,
}

#[derive(Args)]
#[group(skip)]
pub struct Reset {
    #[arg(long, group = "reset_mode")]
    pub soft: bool,
    #[arg(long, group = "reset_mode")]
    pub mixed: bool,
    #[arg(long, group = "reset_mode")]
    pub hard: bool,
    pub revision: Option<String>,
}

#[derive(Args)]
#[group(skip)]
pub struct Clean {
    #[arg(
        short = 'n',
        required_unless_present = "force",
        conflicts_with = "force"
    )]
    pub dry_run: bool,
    #[arg(short = 'f')]
    pub force: bool,
    #[arg(short = 'd', requires = "force", conflicts_with = "dry_run")]
    pub directories: bool,
}

#[derive(Args)]
#[group(skip)]
#[command(
    after_help = "Forms: switch <local-branch> | switch - | switch -c <new-branch> [start-point]\n       switch --detach [revision]\nProtected and other worktrees' branches cannot be switch targets. No remote inference."
)]
pub struct Switch {
    /// Create a new feature branch (never force-reset an existing one).
    #[arg(short = 'c', conflicts_with = "detach")]
    pub create: Option<String>,
    #[arg(long)]
    pub detach: bool,
    /// Local branch, or optional start point with -c/--detach.
    #[arg(required_unless_present_any = ["create", "detach"])]
    pub target: Option<String>,
}

#[derive(Args)]
#[group(skip)]
#[command(
    after_help = "Forms: branch | branch <new-branch> [start-point]\n       branch -d <branch> | branch -D <branch> | branch -m <new-name>"
)]
pub struct Branch {
    #[arg(group = "branch_mode")]
    pub name: Option<String>,
    #[arg(requires = "name")]
    pub start: Option<String>,
    /// Delete a merged, unowned feature branch.
    #[arg(short = 'd', group = "branch_mode")]
    pub delete: Option<String>,
    /// Delete an unowned feature branch even if unmerged.
    #[arg(short = 'D', group = "branch_mode")]
    pub force_delete: Option<String>,
    /// Rename the current feature branch.
    #[arg(short = 'm', group = "branch_mode")]
    pub rename: Option<String>,
}

#[derive(Args)]
#[group(skip)]
#[command(after_help = "Forms: merge [--no-edit] <revision> | merge --continue | merge --abort")]
pub struct Merge {
    #[arg(required_unless_present_any = ["resume", "abort"], group = "merge_mode")]
    pub revision: Option<String>,
    #[arg(long, requires = "revision", conflicts_with_all = ["resume", "abort"])]
    pub no_edit: bool,
    #[arg(long = "continue", group = "merge_mode")]
    pub resume: bool,
    #[arg(long, group = "merge_mode")]
    pub abort: bool,
}

#[derive(Args)]
#[group(skip)]
#[command(
    after_help = "Forms: rebase <upstream> | rebase --onto <newbase> <upstream>\n       rebase --continue | rebase --abort | rebase --skip\nRebases only the current branch with --no-update-refs; protected refs are valid inputs."
)]
pub struct Rebase {
    #[arg(required_unless_present_any = ["resume", "abort", "skip"], group = "rebase_mode")]
    pub upstream: Option<String>,
    #[arg(long, requires = "upstream", conflicts_with_all = ["resume", "abort", "skip"])]
    pub onto: Option<String>,
    #[arg(long = "continue", group = "rebase_mode")]
    pub resume: bool,
    #[arg(long, group = "rebase_mode")]
    pub abort: bool,
    #[arg(long, group = "rebase_mode")]
    pub skip: bool,
}

#[derive(Args)]
#[group(skip)]
pub struct CherryPick {
    #[arg(required_unless_present_any = ["resume", "abort", "skip"], group = "pick_mode")]
    pub commits: Vec<String>,
    #[arg(long = "continue", group = "pick_mode")]
    pub resume: bool,
    #[arg(long, group = "pick_mode")]
    pub abort: bool,
    #[arg(long, group = "pick_mode")]
    pub skip: bool,
}

#[derive(Args)]
#[group(skip)]
pub struct Revert {
    #[arg(required_unless_present_any = ["resume", "abort"], group = "revert_mode")]
    pub commits: Vec<String>,
    #[arg(long, requires = "commits", conflicts_with_all = ["resume", "abort"])]
    pub no_edit: bool,
    #[arg(long = "continue", group = "revert_mode")]
    pub resume: bool,
    #[arg(long, group = "revert_mode")]
    pub abort: bool,
}

#[derive(Subcommand)]
pub enum Worktree {
    List,
    /// Create a feature branch in .local/worktrees/ using an escaped branch name.
    Add {
        #[arg(short = 'b')]
        branch: String,
        start: Option<String>,
    },
    /// Remove the registered branch's worktree only at its AgentTask-managed location.
    Remove {
        branch: String,
    },
}
