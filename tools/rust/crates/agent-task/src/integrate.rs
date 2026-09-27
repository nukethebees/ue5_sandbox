use crate::workspace::{self, branch, protected, query};
use serde_json::Value;
use std::ffi::OsString;
use std::path::{Path, PathBuf};
use std::process::Command;

fn verify_lease(root: &Path) -> Result<(), String> {
    let lease = std::env::var("NUKETHEBEES_JOBSERVER_LEASE")
        .map_err(|_| "Integration requires an exclusive jobserver lease. After user authorization, use integrate-feature.")?;
    let local = std::env::var_os("LOCALAPPDATA")
        .ok_or("LOCALAPPDATA is missing; install the canonical jobserver.")?;
    let output = Command::new(PathBuf::from(local).join("NukeTheBees/jobserver/bin/jobserver.exe"))
        .args(["status", "--json"])
        .output()
        .map_err(|e| format!("Cannot verify integration lease: {e}"))?;
    if !output.status.success() {
        return Err(format!(
            "Cannot verify integration lease: {}",
            String::from_utf8_lossy(&output.stderr)
        ));
    }
    let status: Value = serde_json::from_slice(&output.stdout)
        .map_err(|e| format!("Invalid jobserver status: {e}"))?;
    check_lease(&status, &lease, root)
}

fn check_lease(status: &Value, lease: &str, root: &Path) -> Result<(), String> {
    let job = status["jobs"]
        .as_array()
        .and_then(|jobs| jobs.iter().find(|job| job["id"].as_str() == Some(lease)));
    if let Some(job) = job {
        let worktree = job["worktree"]
            .as_str()
            .and_then(|p| Path::new(p).canonicalize().ok());
        let exclusive = job["claims"].as_array().is_some_and(|claims| {
            claims
                .iter()
                .any(|claim| claim["name"] == "integration/dev" && claim["mode"] == "exclusive")
        });
        if job["state"] == "RUNNING"
            && job["kind"] == "integration"
            && worktree == Some(root.canonicalize().map_err(|e| e.to_string())?)
            && exclusive
        {
            return Ok(());
        }
    }
    Err("Lease is not a running integration job with exclusive integration/dev for this worktree. Use integrate-feature after user authorization.".into())
}

fn clean(root: &Path) -> Result<(), String> {
    if !query(root, &["status", "--porcelain", "--untracked-files=all"])?.is_empty() {
        return Err(format!(
            "Worktree '{}' must have a clean index and working tree before integration.",
            root.display()
        ));
    }
    let admin = query(root, &["rev-parse", "--absolute-git-dir"])?;
    for state in [
        "rebase-merge",
        "rebase-apply",
        "MERGE_HEAD",
        "CHERRY_PICK_HEAD",
        "REVERT_HEAD",
        "sequencer",
        "BISECT_START",
    ] {
        if Path::new(&admin).join(state).exists() {
            return Err(format!(
                "Worktree '{}' has an in-progress Git operation ({state}); finish it outside the integration queue.",
                root.display()
            ));
        }
    }
    Ok(())
}

fn dev_worktree(root: &Path) -> Result<PathBuf, String> {
    let mut matches: Vec<_> = workspace::worktrees(root)?
        .into_iter()
        .filter(|w| w.branch.as_deref() == Some("dev"))
        .map(|w| w.path)
        .collect();
    if matches.len() != 1 {
        return Err("Integration requires exactly one registered worktree on dev. Ask the maintainer to prepare it.".into());
    }
    Ok(matches.remove(0))
}

fn run_git(root: &Path, args: &[&str]) -> Result<(), String> {
    let status = Command::new("git")
        .args(args)
        .current_dir(root)
        .status()
        .map_err(|e| e.to_string())?;
    if !status.success() {
        return Err(format!("git {} failed with {status}", args[0]));
    }
    Ok(())
}

fn home_name(root: &Path) -> Option<&str> {
    root.file_name()
        .and_then(|n| n.to_str())
        .filter(|n| is_home_branch(n))
}

fn is_home_branch(name: &str) -> bool {
    name.strip_prefix("dev")
        .is_some_and(|digits| !digits.is_empty() && digits.bytes().all(|b| b.is_ascii_digit()))
}

fn compare_and_swap(root: &Path, expected: &str, commit: &str) -> Result<(), String> {
    query(root, &["update-ref", "-m", "integrate feature", "refs/heads/dev", commit, expected])
        .map(|_| ()).map_err(|e| format!("Atomic promotion rejected (expected dev {expected}); no retry was attempted. Feature retained. Reinspect dev before requeueing. {e}"))
}

fn transaction(
    root: &Path,
    keep: bool,
    mut verify: impl FnMut(&Path) -> Result<(), String>,
) -> Result<(), String> {
    verify(root)?;
    println!("Integration stage: preflight");
    let feature = branch(root)?;
    if feature.is_empty() || protected(&feature) || is_home_branch(&feature) {
        return Err("Integration requires a feature branch, not detached HEAD, a protected branch, or a devN home branch.".into());
    }
    clean(root)?;
    let dev = dev_worktree(root)?;
    if branch(&dev)? != "dev" {
        return Err("The integration worktree must be on dev.".into());
    }
    clean(&dev)?;
    let base = query(&dev, &["rev-parse", "refs/heads/dev"])?;
    println!("Pinned dev: {base}");
    println!("Integration stage: final-rebase");
    if let Err(error) = run_git(
        root,
        &[
            "rebase",
            "--no-autostash",
            "--no-update-refs",
            "--no-rebase-merges",
            &base,
        ],
    ) {
        let admin = query(root, &["rev-parse", "--absolute-git-dir"])?;
        if ["rebase-merge", "rebase-apply"]
            .iter()
            .any(|p| Path::new(&admin).join(p).exists())
        {
            run_git(root, &["rebase", "--abort"]).map_err(|abort|
                format!("Final rebase failed and abort failed: {abort}. dev was not promoted; feature recovery requires attention outside the queue."))?;
        }
        return Err(format!(
            "Final rebase failed and any active rebase was aborted: {error}. dev was not promoted. Resolve with agent-task git rebase dev outside the queue, validate, then requeue integrate-feature."
        ));
    }
    println!("Integration stage: sanity");
    clean(root)?;
    let tip = query(root, &["rev-parse", "HEAD"])?;
    if tip == base {
        return Err("Feature has no commits to integrate; dev is unchanged.".into());
    }
    run_git(root, &["merge-base", "--is-ancestor", &base, &tip])?;
    run_git(root, &["diff", "--check", &base, &tip])?;
    if branch(&dev)? != "dev" {
        return Err("dev worktree switched branches; promotion stopped.".into());
    }
    let actual = query(&dev, &["rev-parse", "refs/heads/dev"])?;
    if actual != base {
        return Err(format!(
            "Atomic promotion stopped: dev moved from {base} to {actual}. No retry; feature retained. Reinspect and requeue."
        ));
    }
    clean(&dev)?;
    let tree = query(root, &["rev-parse", &format!("{tip}^{{tree}}")])?;
    let message = format!("Merge branch '{feature}' into dev");
    let commit = query(
        root,
        &[
            "commit-tree",
            &tree,
            "-p",
            &base,
            "-p",
            &tip,
            "-m",
            &message,
        ],
    )?;
    println!("Integration stage: atomic-promotion");
    verify(root)?;
    compare_and_swap(&dev, &base, &commit)?;
    println!("dev promoted to {commit}");
    println!("Integration stage: refresh and cleanup");
    let cleanup = || -> Result<(), String> {
        run_git(&dev, &["reset", "--hard", &commit])?;
        if let Some(home) = home_name(root) {
            // Git refuses a home branch already checked out elsewhere; never force it.
            let exists = Command::new("git")
                .args([
                    "show-ref",
                    "--verify",
                    "--quiet",
                    &format!("refs/heads/{home}"),
                ])
                .current_dir(root)
                .status()
                .map_err(|e| e.to_string())?;
            if exists.success() {
                run_git(root, &["switch", home])?;
            } else {
                run_git(root, &["switch", "-c", home, &commit])?;
            }
        } else if !keep {
            run_git(root, &["switch", "--detach", &tip])?;
        }
        if !keep {
            let current = query(root, &["rev-parse", &format!("refs/heads/{feature}")])?;
            if current != tip {
                return Err("Feature moved after promotion; branch retained.".into());
            }
            run_git(&dev, &["branch", "-d", "--", &feature])?;
        }
        Ok(())
    };
    cleanup().map_err(|e| format!("dev already contains merge {commit}, but refresh/cleanup failed: {e}. Feature retained. Ask the maintainer to inspect cleanup; do not repeat integration."))?;
    println!("Integrated {feature} into dev ({commit}).");
    Ok(())
}

pub fn run(args: &[OsString]) -> Result<(), String> {
    if !(args.is_empty() || (args.len() == 1 && args[0] == "--keep-branch")) {
        return Err("Usage: agent-task integrate [--keep-branch]; invoke through authorized integrate-feature.".into());
    }
    workspace::check_environment()?;
    let cwd = std::env::current_dir().map_err(|e| e.to_string())?;
    let root = workspace::root(&cwd)?;
    transaction(&root, !args.is_empty(), verify_lease)
}

#[cfg(test)]
#[path = "../tests/integration/mod.rs"]
mod tests;
