use crate::workspace::{self, protected, query};
use std::ffi::OsString;
use std::path::Path;
use std::process::Command;

fn deny_ref(name: &str) -> Result<(), String> {
    if protected(name) {
        return Err(format!(
            "'{name}' is protected: ordinary Git cannot attach to or modify it. Use a feature branch; advancing dev requires authorized integrate-feature."
        ));
    }
    Ok(())
}

fn ref_target(root: &Path, name: &str) -> Result<(), String> {
    deny_ref(name)?;
    if name == "-" || name.starts_with("@{-") {
        let revision = if name == "-" { "@{-1}" } else { name };
        deny_ref(&query(
            root,
            &["rev-parse", "--symbolic-full-name", revision],
        )?)?;
    }
    Ok(())
}

fn read_only(command: &str) -> bool {
    matches!(
        command,
        "status"
            | "diff"
            | "log"
            | "show"
            | "rev-parse"
            | "merge-base"
            | "ls-files"
            | "ls-tree"
            | "rev-list"
            | "describe"
            | "name-rev"
            | "for-each-ref"
            | "show-ref"
            | "blame"
            | "grep"
            | "shortlog"
            | "help"
            | "version"
    )
}

fn short_flag(arg: &str, flags: &[char]) -> bool {
    arg.starts_with('-') && !arg.starts_with("--") && arg[1..].contains(flags)
}

pub fn check(cwd: &Path, arguments: &[OsString]) -> Result<(), String> {
    workspace::check_environment()?;
    let mut start = 0;
    while let Some(arg) = arguments.get(start).and_then(|a| a.to_str()) {
        if !arg.starts_with('-') {
            break;
        }
        if !matches!(
            arg,
            "--no-pager"
                | "--paginate"
                | "-p"
                | "-P"
                | "--no-optional-locks"
                | "--literal-pathspecs"
                | "--glob-pathspecs"
                | "--noglob-pathspecs"
                | "--icase-pathspecs"
        ) {
            return Err(format!(
                "Global option '{arg}' can redirect or reconfigure Git. Run the command in the current workspace without overrides; repository administration requires the maintainer."
            ));
        }
        start += 1;
    }
    let command = arguments
        .get(start)
        .and_then(|s| s.to_str())
        .ok_or("Use agent-task git <command> <args...>.")?;
    let root = workspace::root(cwd)?;
    let args = &arguments[start + 1..];
    let text: Vec<_> = args.iter().map(|s| s.to_str().unwrap_or("")).collect();
    if matches!(command, "switch" | "checkout" | "worktree")
        && text.iter().take_while(|arg| **arg != "--").any(|arg| {
            arg.len() > 2
                && short_flag(arg, &['b', 'B', 'c', 'C'])
                && !["-b", "-B", "-c", "-C"]
                    .iter()
                    .any(|prefix| arg.starts_with(prefix))
        })
    {
        return Err("Combined branch-creation flags are not inspected. Spell flags separately (for example -f -B feature) so the protected target can be checked.".into());
    }
    let current = workspace::branch(&root)?;
    let mut protected_current = protected(&current);
    if current.is_empty() {
        let admin = query(&root, &["rev-parse", "--absolute-git-dir"])?;
        for state in ["rebase-merge/head-name", "rebase-apply/head-name"] {
            if let Ok(name) = std::fs::read_to_string(Path::new(&admin).join(state)) {
                protected_current |= protected(name.trim());
            }
        }
    }
    if protected_current && !read_only(command) {
        return Err("Current worktree is on a protected branch. Only read-only inspection is available; use your feature worktree or ask the maintainer.".into());
    }
    if matches!(
        command,
        "init"
            | "clone"
            | "config"
            | "remote"
            | "symbolic-ref"
            | "fetch"
            | "pull"
            | "push"
            | "receive-pack"
            | "upload-pack"
            | "send-pack"
            | "fast-import"
            | "fast-export"
            | "filter-branch"
            | "filter-repo"
            | "maintenance"
            | "gc"
            | "prune"
            | "repack"
            | "multi-pack-index"
    ) {
        return Err(format!(
            "'{command}' exposes repository administration or arbitrary ref destinations. Ask the maintainer; ordinary feature edits use commit/branch/rebase/merge."
        ));
    }
    let alias = Command::new("git")
        .args(["config", "--get", &format!("alias.{command}")])
        .current_dir(cwd)
        .output()
        .map_err(|e| e.to_string())?;
    if alias.status.success() {
        return Err("Git aliases are not inspected. Invoke the underlying Git command through agent-task git.".into());
    }

    // These options explicitly write files; ordinary revision/pathspec arguments are left to Git.
    for (i, arg) in text.iter().enumerate() {
        if *arg == "--" {
            break;
        }
        if command == "checkout-index" && arg.starts_with("--prefix=") {
            workspace::contained(&root, cwd, arg[9..].as_ref())?;
        } else if command == "checkout-index" && *arg == "--prefix" {
            workspace::contained(
                &root,
                cwd,
                args.get(i + 1)
                    .ok_or("--prefix needs a workspace destination.")?,
            )?;
        }
        if (command == "apply" && *arg == "--unsafe-paths")
            || (command == "archive" && (*arg == "--remote" || arg.starts_with("--remote=")))
        {
            return Err("This option permits access outside the workspace. Use local Git paths without redirection; external operations require the maintainer.".into());
        }
        if !matches!(
            command,
            "diff"
                | "log"
                | "show"
                | "diff-tree"
                | "diff-files"
                | "diff-index"
                | "format-patch"
                | "archive"
        ) {
            continue;
        }
        if protected_current && (*arg == "--output" || arg.starts_with("--output=")) {
            return Err("Output files would mutate the protected worktree. Read the result on stdout, or use your feature worktree.".into());
        }
        if let Some(path) = arg.strip_prefix("--output=") {
            workspace::contained(&root, cwd, path.as_ref())?;
        } else if *arg == "--output"
            || (command == "format-patch" && matches!(*arg, "-o" | "--output-directory"))
        {
            let path = args
                .get(i + 1)
                .ok_or("Output option needs a workspace destination.")?;
            workspace::contained(&root, cwd, path)?;
        } else if command == "format-patch" && arg.starts_with("--output-directory=") {
            workspace::contained(&root, cwd, arg[19..].as_ref())?;
        } else if matches!(command, "archive" | "format-patch") && arg.starts_with("-o") {
            let path = if *arg == "-o" {
                args.get(i + 1)
                    .ok_or("-o needs a destination.")?
                    .as_os_str()
            } else {
                arg[2..].as_ref()
            };
            workspace::contained(&root, cwd, path)?;
        }
    }
    match command {
        "switch" | "checkout" => {
            let mut creating = false;
            let detached = text.iter().any(|arg| matches!(*arg, "--detach" | "-d"));
            let tracking = text
                .iter()
                .any(|arg| matches!(*arg, "-t" | "--track") || arg.starts_with("--track="));
            let mut target_seen = false;
            let mut i = 0;
            while i < text.len() {
                let arg = text[i];
                if arg == "--" {
                    if command == "switch" && !creating && !detached {
                        if let Some(name) = text.get(i + 1) {
                            if tracking {
                                deny_ref(name.rsplit('/').next().unwrap_or(name))?;
                            }
                            ref_target(&root, name)?;
                        }
                    }
                    break;
                }
                if matches!(
                    arg,
                    "-c" | "-C" | "-b" | "-B" | "--create" | "--force-create" | "--orphan"
                ) {
                    i += 1;
                    if let Some(name) = text.get(i) {
                        deny_ref(name)?;
                    }
                    creating = true;
                } else if let Some((option, name)) = arg.split_once('=') {
                    if matches!(option, "--create" | "--force-create" | "--orphan") {
                        deny_ref(name)?;
                        creating = true;
                    }
                } else if ["-b", "-B", "-c", "-C"]
                    .iter()
                    .any(|p| arg.starts_with(p) && arg.len() > 2)
                {
                    deny_ref(&arg[2..])?;
                    creating = true;
                } else if (!arg.starts_with('-') || arg == "-") && !target_seen {
                    // A start point for a new feature is only an input. Path checkout also is.
                    let paths = command == "checkout"
                        && text
                            .iter()
                            .position(|arg| *arg == "--")
                            .is_some_and(|separator| separator + 1 < text.len());
                    if !creating && !paths && !detached {
                        if tracking {
                            deny_ref(arg.rsplit('/').next().unwrap_or(arg))?;
                        }
                        ref_target(&root, arg)?;
                    }
                    target_seen = true;
                }
                i += 1;
            }
        }
        "branch" => {
            let listing = text.iter().any(|a| {
                matches!(
                    *a,
                    "--list" | "--show-current" | "--contains" | "--merged" | "--no-merged"
                )
            });
            if !listing {
                let moving = text.iter().any(|a| {
                    short_flag(a, &['m', 'M', 'c', 'C', 'd', 'D'])
                        || matches!(
                            *a,
                            "-m" | "-M"
                                | "--move"
                                | "-c"
                                | "-C"
                                | "--copy"
                                | "-d"
                                | "-D"
                                | "--delete"
                        )
                });
                let mut targets = text.iter().filter(|a| !a.starts_with('-'));
                if text
                    .iter()
                    .any(|arg| short_flag(arg, &['c', 'C']) || *arg == "--copy")
                {
                    if let Some(name) = targets.last() {
                        deny_ref(name)?;
                    }
                } else {
                    if let Some(name) = targets.next() {
                        deny_ref(name)?;
                    }
                    if moving {
                        for name in targets {
                            deny_ref(name)?;
                        }
                    }
                }
            }
        }
        "update-ref" => {
            if text.contains(&"--stdin") {
                return Err("update-ref --stdin can write arbitrary refs. Use a direct non-protected feature ref, or ask the maintainer.".into());
            }
            let mut skip = false;
            for arg in &text {
                if skip {
                    skip = false;
                    continue;
                }
                if *arg == "-m" {
                    skip = true;
                    continue;
                }
                if arg.starts_with('-') {
                    continue;
                }
                if !arg.starts_with("refs/heads/") && !arg.starts_with("refs/tags/") {
                    return Err("update-ref requires an explicit non-protected refs/heads/ or refs/tags/ target. Use reset for current feature HEAD.".into());
                }
                deny_ref(arg)?;
                break;
            }
        }
        "rebase" => {
            let configured = Command::new("git")
                .args(["config", "--bool", "--get", "rebase.updateRefs"])
                .current_dir(cwd)
                .output()
                .map_err(|e| e.to_string())?;
            if text.contains(&"--update-refs")
                || (configured.stdout.starts_with(b"true") && !text.contains(&"--no-update-refs"))
            {
                return Err(
                    "rebase update-refs can move protected branches. Use rebase --no-update-refs."
                        .into(),
                );
            }
            let mut operands = Vec::new();
            let mut skip = false;
            for arg in &text {
                if skip {
                    skip = false;
                    continue;
                }
                if matches!(
                    *arg,
                    "--onto" | "--exec" | "-x" | "--strategy" | "-s" | "--strategy-option" | "-X"
                ) {
                    skip = true;
                } else if !arg.starts_with('-') {
                    operands.push(*arg);
                }
            }
            if operands.len() >= 2 || (text.contains(&"--root") && !operands.is_empty()) {
                ref_target(&root, operands[operands.len() - 1])?;
            }
        }
        "worktree" => check_worktree(&root, cwd, args)?,
        _ => {}
    }
    Ok(())
}

fn check_worktree(root: &Path, cwd: &Path, args: &[OsString]) -> Result<(), String> {
    let sub = args.first().and_then(|a| a.to_str()).unwrap_or("");
    if sub == "list" {
        return Ok(());
    }
    if !matches!(sub, "add" | "move" | "remove" | "lock" | "unlock") {
        return Err("This worktree administration command can affect other workspaces. Ask the maintainer; add/move/remove accept paths inside this workspace.".into());
    }
    let mut operands = Vec::new();
    let mut branch = false;
    let mut explicit_branch = false;
    let detached = args.iter().any(|arg| arg == "--detach" || arg == "-d");
    let mut skip = false;
    let mut options = true;
    for arg in &args[1..] {
        let text = arg.to_str().unwrap_or("");
        if branch {
            deny_ref(text)?;
            branch = false;
            continue;
        }
        if skip {
            skip = false;
            continue;
        }
        if options && text == "--" {
            options = false;
            continue;
        }
        if options && matches!(text, "-b" | "-B" | "--orphan") {
            branch = true;
            explicit_branch = true;
            continue;
        }
        if options && text == "--reason" {
            skip = true;
            continue;
        }
        if options && (text.starts_with("-b") || text.starts_with("-B")) {
            deny_ref(&text[2..])?;
            explicit_branch = true;
            continue;
        }
        if options && text.starts_with("--orphan=") {
            deny_ref(&text[9..])?;
            explicit_branch = true;
            continue;
        }
        if options && text.starts_with('-') {
            continue;
        }
        operands.push(arg);
    }
    let count = if sub == "move" { 2 } else { 1 };
    for path in operands.iter().take(count) {
        workspace::contained(root, cwd, path)?;
    }
    if sub != "add" {
        if let Some(path) = operands.first() {
            let source = workspace::resolved(&cwd.join(path))?;
            if !source.is_dir()
                || workspace::root(&source)?
                    .canonicalize()
                    .map_err(|e| e.to_string())?
                    != source
                || query(
                    &source,
                    &["rev-parse", "--path-format=absolute", "--git-common-dir"],
                )? != query(
                    root,
                    &["rev-parse", "--path-format=absolute", "--git-common-dir"],
                )?
            {
                return Err("Worktree source must name an existing worktree of this repository inside the workspace. Git's shorthand lookup could select another workspace; use its full path.".into());
            }
            deny_ref(&workspace::branch(&source)?)?;
        }
    }
    if sub == "add" && !explicit_branch && !detached {
        if let Some(name) = operands.get(1) {
            ref_target(root, &name.to_string_lossy())?;
        }
        // Git can infer/create the branch from the destination's basename.
        if operands.len() == 1
            && let Some(path) = operands.first()
        {
            if let Some(name) = Path::new(path).file_name() {
                deny_ref(&name.to_string_lossy())?;
            }
        }
    }
    Ok(())
}

pub fn run(arguments: &[OsString]) -> Result<i32, String> {
    let cwd = std::env::current_dir().map_err(|e| e.to_string())?;
    check(&cwd, arguments)?;
    let status = Command::new("git")
        .args(arguments)
        .status()
        .map_err(|e| format!("Could not start Git: {e}"))?;
    Ok(status.code().unwrap_or(1))
}
