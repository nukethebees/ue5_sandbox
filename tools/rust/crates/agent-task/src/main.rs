use std::fs;
use std::io::ErrorKind;
use std::path::{Path, PathBuf};
use std::process::{Command, ExitCode};

mod git;
mod integrate;
mod workspace;

const USAGE: &str = "Usage: agent-task <command>\n\nCommands:\n  prepare-worktree       Clean and initialize the current worktree\n  install-central-tools  Install/update canonical per-user build tools\n  git <args...>          Run Git within feature-workspace guardrails\n  integrate [--keep-branch]  Privileged dev transaction; use authorized integrate-feature";

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

fn require_jobserver() -> Result<(), String> {
    let local_app_data = std::env::var_os("LOCALAPPDATA")
        .filter(|value| !value.is_empty())
        .ok_or("LOCALAPPDATA is not set. Set it and run 'agent-task install-central-tools'.")?;
    let jobserver = PathBuf::from(local_app_data).join("NukeTheBees/jobserver/bin/jobserver.exe");
    if !jobserver.is_file() {
        return Err(format!(
            "Canonical jobserver is missing at '{}'. Run 'agent-task install-central-tools' first.",
            jobserver.display()
        ));
    }
    Ok(())
}

fn prepare_worktree() -> Result<(), String> {
    let root = worktree_root()?;
    require_jobserver()?;

    println!("[1/5] Clearing build output");
    let output = root.join("out");
    match fs::remove_dir_all(&output) {
        Ok(()) => {}
        Err(error) if error.kind() == ErrorKind::NotFound => {}
        Err(error) => return Err(format!("Could not remove '{}': {error}", output.display())),
    }

    println!("[2/5] Synchronizing submodules");
    run(&root, "git", &["submodule", "sync", "--recursive"])?;

    println!("[3/5] Updating submodules");
    run(
        &root,
        "git",
        &["submodule", "update", "--init", "--recursive"],
    )?;

    println!("[4/5] Generating CMake presets");
    run(&root, "python", &["cmake/presets/generate.py"])?;

    println!("[5/5] Generating code");
    run(&root, "cmake", &["--workflow", "--preset", "generate-code"])
}

fn install_central_tools() -> Result<(), String> {
    let root = worktree_root()?;

    println!("[1/6] Synchronizing submodules");
    run(&root, "git", &["submodule", "sync", "--recursive"])?;

    println!("[2/6] Updating submodules");
    run(
        &root,
        "git",
        &["submodule", "update", "--init", "--recursive"],
    )?;

    println!("[3/6] Generating CMake presets");
    run(&root, "python", &["cmake/presets/generate.py"])?;

    println!("[4/6] Configuring native build");
    run(&root, "cmake", &["--preset", "native"])?;

    println!("[5/6] Installing canonical jobserver");
    run(
        &root,
        "cmake",
        &[
            "--build",
            "--preset",
            "native",
            "--target",
            "install-jobserver",
        ],
    )?;

    println!("[6/6] Installing canonical set-live-coding-disabled");
    run(
        &root,
        "cmake",
        &[
            "--build",
            "--preset",
            "native",
            "--target",
            "install-set-live-coding-disabled",
        ],
    )
}

fn main() -> ExitCode {
    let arguments = std::env::args_os().skip(1).collect::<Vec<_>>();
    if arguments.first().is_some_and(|arg| arg == "git") {
        match git::run(&arguments[1..]) {
            Ok(code) => std::process::exit(code),
            Err(error) => {
                eprintln!("agent-task: {error}");
                return ExitCode::FAILURE;
            }
        }
    }
    if arguments.first().is_some_and(|arg| arg == "integrate") {
        return match integrate::run(&arguments[1..]) {
            Ok(()) => ExitCode::SUCCESS,
            Err(error) => {
                eprintln!("agent-task: {error}");
                ExitCode::FAILURE
            }
        };
    }
    if arguments.len() == 1 && (arguments[0] == "--help" || arguments[0] == "-h") {
        println!("{USAGE}");
        return ExitCode::SUCCESS;
    }
    if arguments.len() != 1 {
        eprintln!("{USAGE}");
        return ExitCode::from(2);
    }

    let result = if arguments[0] == "prepare-worktree" {
        prepare_worktree()
    } else if arguments[0] == "install-central-tools" {
        install_central_tools()
    } else {
        eprintln!("{USAGE}");
        return ExitCode::from(2);
    };

    match result {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("agent-task: {error}");
            ExitCode::FAILURE
        }
    }
}
