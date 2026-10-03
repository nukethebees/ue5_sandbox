use crate::workspace;
use clap::Parser;
use std::collections::BTreeSet;
use std::ffi::OsString;
use std::fs;
use std::path::{Component, Path, PathBuf};
use std::process::Command;

#[derive(Parser)]
#[command(
    name = "coj format",
    about = "Format project sources using .code-format.json (default: all)"
)]
struct Arguments {
    #[arg(long, group = "selection")]
    all: bool,
    #[arg(long, group = "selection")]
    changed: bool,
    #[arg(long, group = "selection")]
    staged: bool,
    #[arg(long, short = 'j', default_value_t = default_jobs(), value_parser = clap::value_parser!(u32).range(1..))]
    jobs: u32,
    #[arg(long)]
    verbose: bool,
}

fn default_jobs() -> u32 {
    (std::thread::available_parallelism().map_or(1, usize::from) / 2).clamp(1, 16) as u32
}

struct Policy {
    roots: Vec<PathBuf>,
    extensions: Vec<String>,
    excluded: Vec<String>,
}

impl Policy {
    fn load(root: &Path) -> Result<Self, String> {
        let path = root.join(".code-format.json");

        let json: serde_json::Value = serde_json::from_slice(
            &fs::read(&path).map_err(|e| format!("Cannot read {}: {e}", path.display()))?,
        )
        .map_err(|e| format!("Invalid formatting policy: {e}"))?;

        let strings = |name: &str| -> Result<Vec<String>, String> {
            let values: Vec<String> = serde_json::from_value(json[name].clone())
                .map_err(|e| format!("Invalid formatting policy {name}: {e}"))?;

            if values.iter().any(|s| s.trim().is_empty()) {
                return Err(format!("Formatting policy {name} contains an empty value"));
            }

            Ok(values)
        };

        let roots = strings("Roots")?
            .into_iter()
            .map(PathBuf::from)
            .collect::<Vec<_>>();

        for path in &roots {
            if path.components().any(|c| {
                matches!(
                    c,
                    Component::Prefix(_) | Component::RootDir | Component::ParentDir
                )
            }) {
                return Err(format!(
                    "Formatting root must stay inside the worktree: {}",
                    path.display()
                ));
            }

            workspace::contained(root, root, path.as_os_str())?;
        }

        let roots = roots
            .into_iter()
            .map(|p| p.components().filter(|c| *c != Component::CurDir).collect())
            .collect();

        Ok(Self {
            roots,
            extensions: strings("Extensions")?,
            excluded: strings("ExcludedComponents")?,
        })
    }

    fn excluded(&self, path: &Path) -> bool {
        path.components().any(|c| {
            self.excluded
                .iter()
                .any(|s| c.as_os_str().to_string_lossy().eq_ignore_ascii_case(s))
        })
    }

    fn candidate(&self, root: &Path, path: &Path) -> bool {
        root.join(path).is_file()
            && self.roots.iter().any(|r| path.starts_with(r))
            && !self.excluded(path)
            && path.extension().is_some_and(|ext| {
                self.extensions
                    .iter()
                    .any(|s| s.eq_ignore_ascii_case(&format!(".{}", ext.to_string_lossy())))
            })
    }

    fn scan(&self, root: &Path, path: &Path, files: &mut BTreeSet<PathBuf>) -> Result<(), String> {
        if self.excluded(path) {
            return Ok(());
        }

        for entry in fs::read_dir(root.join(path))
            .map_err(|e| format!("Cannot scan {}: {e}", path.display()))?
        {
            let entry = entry.map_err(|e| e.to_string())?;
            let kind = entry.file_type().map_err(|e| e.to_string())?;
            let relative = path.join(entry.file_name());
            // Do not follow directory links into another workspace or recurse through cycles.
            if kind.is_dir() {
                self.scan(root, &relative, files)?;
            } else if kind.is_file() && self.candidate(root, &relative) {
                files.insert(relative);
            }
        }

        Ok(())
    }
}

fn git_paths(root: &Path, args: &[&str]) -> Result<BTreeSet<PathBuf>, String> {
    Ok(workspace::query(root, args)?
        .split('\0')
        .filter(|s| !s.is_empty())
        .map(PathBuf::from)
        .collect())
}

fn format_file(root: &Path, executable: &Path, path: &Path) -> Result<(), String> {
    let output = Command::new(executable)
        .arg("-i")
        .arg(path)
        .current_dir(root)
        .output()
        .map_err(|e| format!("Cannot run clang-format: {e}"))?;
    if !output.status.success() {
        return Err(format!(
            "clang-format {}: {}",
            output.status,
            String::from_utf8_lossy(&output.stderr).trim()
        ));
    }

    let path = root.join(path);
    let contents = fs::read(&path).map_err(|e| e.to_string())?;

    if contents.contains(&b'\r') {
        let mut normalized = Vec::with_capacity(contents.len());
        let mut bytes = contents.iter().peekable();

        while let Some(&byte) = bytes.next() {
            normalized.push(if byte == b'\r' { b'\n' } else { byte });
            if byte == b'\r' && bytes.peek() == Some(&&b'\n') {
                bytes.next();
            }
        }

        fs::write(path, normalized).map_err(|e| e.to_string())?;
    }

    Ok(())
}

pub fn run(arguments: &[OsString]) -> Result<i32, String> {
    let args = match Arguments::try_parse_from(
        std::iter::once(OsString::from("coj format")).chain(arguments.iter().cloned()),
    ) {
        Ok(args) => args,
        Err(error) => {
            let code = error.exit_code();
            let _ = error.print();
            return Ok(code);
        }
    };

    workspace::check_environment()?;
    let root = workspace::root(&std::env::current_dir().map_err(|e| e.to_string())?)?;

    if args.staged {
        workspace::check_branch_target(
            &workspace::branch(&root)?,
            &root,
            &workspace::worktrees(&root)?,
        )?;
    }

    let policy = Policy::load(&root)?;
    let mut files = BTreeSet::new();

    if args.changed || args.staged {
        files.extend(git_paths(
            &root,
            &["diff", "--cached", "--name-only", "-z"],
        )?);
        if args.changed {
            files.extend(git_paths(&root, &["diff", "--name-only", "-z"])?);
            files.extend(git_paths(
                &root,
                &["ls-files", "--others", "--exclude-standard", "-z"],
            )?);
        }

        files.retain(|p| policy.candidate(&root, p));
    } else {
        for path in &policy.roots {
            if !root.join(path).is_dir() {
                eprintln!("WARNING: Directory not found: {}", path.display());
                continue;
            }
            policy.scan(&root, path, &mut files)?;
        }
    }

    for path in &files {
        workspace::contained(&root, &root, path.as_os_str())?;
    }

    if args.staged {
        let unstaged = git_paths(&root, &["diff", "--name-only", "-z"])?;
        let conflicts = files
            .intersection(&unstaged)
            .map(|p| p.display().to_string())
            .collect::<Vec<_>>();

        if !conflicts.is_empty() {
            return Err(format!(
                "Staged files also have unstaged edits; stage or commit those edits first:\n  {}",
                conflicts.join("\n  ")
            ));
        }
    }

    if files.is_empty() {
        println!("No files to format.");
        return Ok(0);
    }

    let executable = std::env::var_os("LLVM_ROOT")
        .filter(|s| !s.is_empty())
        .map_or_else(
            || PathBuf::from("clang-format"),
            |p| {
                PathBuf::from(p).join("bin").join(if cfg!(windows) {
                    "clang-format.exe"
                } else {
                    "clang-format"
                })
            },
        );

    let files = files.into_iter().collect::<Vec<_>>();
    let jobs = (args.jobs as usize).min(files.len());

    let results = std::thread::scope(|scope| {
        let workers = (0..jobs)
            .map(|worker| {
                let (files, root, executable) = (&files, &root, &executable);
                scope.spawn(move || {
                    // Distribute files by stride to avoid a shared work queue.
                    (worker..files.len())
                        .step_by(jobs)
                        .map(|index| (index, format_file(root, executable, &files[index])))
                        .collect::<Vec<_>>()
                })
            })
            .collect::<Vec<_>>();

        workers
            .into_iter()
            .flat_map(|worker| worker.join().expect("formatter worker panicked"))
            .collect::<Vec<_>>()
    });

    let mut failed = false;

    for (index, result) in results {
        if args.verbose {
            println!("Formatting: {}", files[index].display());
        }
        if let Err(error) = result {
            eprintln!("ERROR formatting {}: {error}", files[index].display());
            failed = true;
        }
    }

    if failed {
        return Ok(1);
    }

    if args.staged {
        // Use the existing constrained Git path, including its workspace/branch checks.
        let arguments = std::iter::once(OsString::from("add"))
            .chain(std::iter::once(OsString::from("--")))
            .chain(files.iter().map(|p| root.join(p).into_os_string()))
            .collect::<Vec<_>>();
        let code = crate::git::run(&arguments)?;
        if code != 0 {
            return Ok(code);
        }
    }

    println!("Successfully formatted {} files.", files.len());
    Ok(0)
}
