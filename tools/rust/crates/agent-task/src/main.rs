use std::fs;
use std::io::ErrorKind;
use std::path::{Path, PathBuf};
use std::process::{Command, ExitCode};

fn worktree_root() -> Result<PathBuf, String> {
    let output = Command::new("git")
        .args(["rev-parse", "--show-toplevel"])
        .output()
        .map_err(|error| format!("Could not determine the Git worktree root: {error}"))?;

    if !output.status.success() {
        return Err(format!(
            "Could not determine the Git worktree root; run inside a Git worktree.\n{}",
            String::from_utf8_lossy(&output.stderr).trim_end()
        ));
    }

    let root = String::from_utf8(output.stdout)
        .map_err(|error| format!("Invalid Git worktree path: {error}"))?;
    Ok(PathBuf::from(root.trim_end_matches(['\r', '\n'])))
}

fn run(root: &Path, program: &str, arguments: &[&str]) -> Result<(), String> {
    let status = Command::new(program)
        .args(arguments)
        .current_dir(root)
        .status()
        .map_err(|error| format!("Could not run {program}: {error}"))?;

    if !status.success() {
        return Err(format!("{program} failed with {status}"));
    }
    Ok(())
}

fn start() -> Result<(), String> {
    let root = worktree_root()?;

    println!("[1/6] Clearing build output");
    let output = root.join("out");
    match fs::remove_dir_all(&output) {
        Ok(()) => {}
        Err(error) if error.kind() == ErrorKind::NotFound => {}
        Err(error) => return Err(format!("Could not remove '{}': {error}", output.display())),
    }

    println!("[2/6] Synchronizing submodules");
    run(&root, "git", &["submodule", "sync", "--recursive"])?;

    println!("[3/6] Updating submodules");
    run(
        &root,
        "git",
        &["submodule", "update", "--init", "--recursive"],
    )?;

    println!("[4/6] Generating CMake presets");
    run(&root, "python", &["cmake/presets/generate.py"])?;

    println!("[5/6] Generating code");
    run(&root, "cmake", &["--workflow", "--preset", "generate-code"])?;

    println!("[6/6] Building task-start baseline");
    run(&root, "cmake", &["--workflow", "--preset", "task-start"])
}

fn main() -> ExitCode {
    let arguments = std::env::args_os().skip(1).collect::<Vec<_>>();
    if arguments.len() == 1 && (arguments[0] == "--help" || arguments[0] == "-h") {
        println!("Usage: agent-task start\n\nClean and initialize the current Git worktree.");
        return ExitCode::SUCCESS;
    }
    if arguments.len() != 1 || arguments[0] != "start" {
        eprintln!("Usage: agent-task start");
        return ExitCode::from(2);
    }

    match start() {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("agent-task: {error}");
            ExitCode::FAILURE
        }
    }
}
