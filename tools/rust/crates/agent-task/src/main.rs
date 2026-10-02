use std::fs;
use std::io::ErrorKind;
use std::path::{Path, PathBuf};
use std::process::{Command, ExitCode};

mod format;

mod benchmark;

mod unreal;

mod git;

mod git_cli;

mod integrate;

mod jobs;

mod live_coding;

mod presets;

mod tidy;

mod unreal_build;

mod workspace;

mod codex;

const USAGE: &str = "Usage: agent-task <command>\n\nCommands:\n  presets [--check]      Generate/check CMake presets\n  tidy [options]        Run LLVM analysis\n  unreal <operation>    Generate project files or authored assets\n  editor [options]      Build and launch the editor\n  run-staged [options]  Launch an existing staged game\n  benchmark <operation> Delegate to revision-local benchmark-tools\n  format [options]      Format sources using revision-local policy\n  unreal-build [options] Invoke the Unreal build script\n  prepare-worktree       Clean and initialize the current worktree\n  install-central-tools  Install/update canonical per-user build tools\n  codex <command>        Launch, inspect, or clean named Codex sessions\n  jobs <command>         Cooperative jobs board (agent-task jobs --help)\n  git <command>          Run supported feature Git operations (agent-task git --help)\n  integrate [--keep-branch]  Privileged dev transaction; explicit user authorization required";

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

fn update_submodules(root: &Path) -> Result<(), String> {
    println!("Synchronizing submodules");
    run(root, "git", &["submodule", "sync", "--recursive"])?;

    println!("Updating submodules");
    run(
        root,
        "git",
        &["submodule", "update", "--init", "--recursive"],
    )
}

fn prepare_worktree() -> Result<(), String> {
    let root = worktree_root()?;

    println!("[1/5] Clearing build output");
    let output = root.join("out");
    match fs::remove_dir_all(&output) {
        Ok(()) => {}
        Err(error) if error.kind() == ErrorKind::NotFound => {}
        Err(error) => return Err(format!("Could not remove '{}': {error}", output.display())),
    }

    println!("[2/5] Preparing submodules");
    update_submodules(&root)?;

    println!("[3/5] Generating CMake presets");
    presets::generate(&root, false)?;

    println!("[4/5] Disabling Live Coding if saved settings exist");
    if let Err(error) = live_coding::disable(
        &root.join("Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini"),
    ) {
        eprintln!("agent-task: warning: Could not disable Live Coding in saved settings: {error}");
    }

    println!("[5/5] Generating code");
    run(&root, "cmake", &["--workflow", "--preset", "generate-code"])
}

fn install_central_tools() -> Result<(), String> {
    let root = worktree_root()?;

    println!("[1/3] Preparing submodules");
    update_submodules(&root)?;

    println!("[2/3] Configuring native build (run prepare-worktree first)");
    run(&root, "cmake", &["--preset", "native"])?;

    println!("[3/3] Installing canonical jobserver");
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
    )
}

fn main() -> ExitCode {
    let arguments = std::env::args_os().skip(1).collect::<Vec<_>>();

    if arguments.first().is_some_and(|arg| arg == "codex") {
        return match codex::run(&arguments[1..]) {
            Ok(code) => std::process::exit(code),
            Err(error) => {
                eprintln!("agent-task: {error}");
                ExitCode::FAILURE
            }
        };
    }

    if arguments.first().is_some_and(|arg| {
        matches!(
            arg.to_str(),
            Some(
                "format"
                    | "unreal-build"
                    | "presets"
                    | "tidy"
                    | "unreal"
                    | "editor"
                    | "run-staged"
                    | "benchmark"
            )
        )
    }) {
        let result = match arguments[0].to_str().unwrap() {
            "format" => format::run(&arguments[1..]),
            "presets" => presets::run(&arguments[1..]),
            "tidy" => tidy::run(&arguments[1..]),
            "unreal" => unreal::run(&arguments[1..]),
            "editor" => unreal::editor(&arguments[1..]),
            "run-staged" => unreal::run_staged(&arguments[1..]),
            "benchmark" => benchmark::run(&arguments[1..]),
            _ => unreal_build::run(&arguments[1..]),
        };
        match result {
            Ok(code) => std::process::exit(code),
            Err(error) => {
                eprintln!("agent-task: {error}");
                return ExitCode::FAILURE;
            }
        }
    }

    if arguments.first().is_some_and(|arg| arg == "jobs") {
        return match jobs::run(&arguments[1..]) {
            Ok(code) => ExitCode::from(code as u8),
            Err(error) => {
                eprintln!("agent-task: {error}");
                ExitCode::FAILURE
            }
        };
    }

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

    if arguments.len() == 1 && arguments[0] == "--version" {
        println!("agent-task {}", env!("CARGO_PKG_VERSION"));
        return ExitCode::SUCCESS;
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
