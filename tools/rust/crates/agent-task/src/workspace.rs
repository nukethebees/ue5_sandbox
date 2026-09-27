use std::ffi::OsStr;
use std::path::{Component, Path, PathBuf};
use std::process::Command;

pub fn query(root: &Path, args: &[&str]) -> Result<String, String> {
    let output = Command::new("git")
        .args(args)
        .current_dir(root)
        .output()
        .map_err(|e| format!("Could not run Git: {e}"))?;
    if !output.status.success() {
        return Err(format!(
            "git {} failed: {}",
            args[0],
            String::from_utf8_lossy(&output.stderr).trim()
        ));
    }
    String::from_utf8(output.stdout)
        .map(|s| s.trim_end_matches(['\r', '\n']).to_owned())
        .map_err(|e| format!("Git metadata is not UTF-8: {e}"))
}

pub fn root(cwd: &Path) -> Result<PathBuf, String> {
    query(cwd, &["rev-parse", "--show-toplevel"]).map(PathBuf::from)
}

pub fn protected(name: &str) -> bool {
    matches!(
        name.strip_prefix("refs/heads/").unwrap_or(name),
        "dev" | "main" | "master"
    )
}

pub fn branch(root: &Path) -> Result<String, String> {
    query(root, &["branch", "--show-current"])
}

pub struct Worktree {
    pub path: PathBuf,
    pub branch: Option<String>,
}

pub fn worktrees(root: &Path) -> Result<Vec<Worktree>, String> {
    let listing = query(root, &["worktree", "list", "--porcelain", "-z"])?;
    let mut result: Vec<Worktree> = Vec::new();
    for field in listing.split('\0') {
        if let Some(path) = field.strip_prefix("worktree ") {
            result.push(Worktree {
                path: resolved(Path::new(path))?,
                branch: None,
            });
        } else if let Some(branch) = field.strip_prefix("branch refs/heads/") {
            if let Some(worktree) = result.last_mut() {
                worktree.branch = Some(branch.to_owned());
            }
        }
    }
    Ok(result)
}

pub fn check_branch_target(name: &str, root: &Path, worktrees: &[Worktree]) -> Result<(), String> {
    if protected(name) {
        return Err(format!(
            "Branch '{name}' is protected. Use a feature branch; advancing dev requires authorized integrate-feature."
        ));
    }
    let root = resolved(root)?;
    if let Some(owner) = worktrees
        .iter()
        .find(|w| w.branch.as_deref() == Some(name) && w.path != root)
    {
        return Err(format!(
            "Branch '{name}' belongs to another worktree ({}). Use your own feature branch; ask the maintainer to coordinate changes there.",
            owner.path.display()
        ));
    }
    Ok(())
}

pub fn check_environment() -> Result<(), String> {
    for name in [
        "GIT_DIR",
        "GIT_WORK_TREE",
        "GIT_COMMON_DIR",
        "GIT_INDEX_FILE",
        "GIT_OBJECT_DIRECTORY",
        "GIT_ALTERNATE_OBJECT_DIRECTORIES",
        "GIT_NAMESPACE",
        "GIT_CONFIG",
        "GIT_CONFIG_COUNT",
        "GIT_CONFIG_PARAMETERS",
    ] {
        if std::env::var_os(name).is_some() {
            return Err(format!(
                "{name} overrides the current workspace. Unset it and retry agent-task."
            ));
        }
    }
    Ok(())
}

// Resolve existing ancestors too, so a linked directory cannot expand the workspace boundary.
pub fn resolved(path: &Path) -> Result<PathBuf, String> {
    let mut result = PathBuf::new();
    for part in path.components() {
        match part {
            Component::ParentDir => {
                result.pop();
            }
            Component::CurDir => {}
            other => {
                result.push(other);
                if result.exists() {
                    result = result
                        .canonicalize()
                        .map_err(|e| format!("Cannot resolve {}: {e}", result.display()))?;
                }
            }
        }
    }
    Ok(result)
}

pub fn contained(root: &Path, cwd: &Path, destination: &OsStr) -> Result<(), String> {
    let target = resolved(&cwd.join(destination))?;
    let root = root.canonicalize().map_err(|e| e.to_string())?;
    if !target.starts_with(&root) {
        return Err(format!(
            "Destination '{}' must be inside workspace '{}'. Use a child path; work outside this boundary requires maintainer intervention.",
            target.display(),
            root.display()
        ));
    }
    Ok(())
}
