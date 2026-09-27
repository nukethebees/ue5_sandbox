use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Command, Output};
use std::sync::atomic::{AtomicUsize, Ordering};

static NEXT_DIRECTORY: AtomicUsize = AtomicUsize::new(0);

struct TemporaryDirectory(PathBuf);

impl TemporaryDirectory {
    fn new() -> Self {
        let sequence = NEXT_DIRECTORY.fetch_add(1, Ordering::Relaxed);
        let path =
            std::env::temp_dir().join(format!("agent-task test {}-{sequence}", std::process::id()));
        fs::create_dir(&path).unwrap();
        Self(path)
    }
}

impl Drop for TemporaryDirectory {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.0);
    }
}

fn git(root: &Path, arguments: &[&str]) {
    let output = Command::new("git")
        .args(arguments)
        .current_dir(root)
        .output()
        .unwrap();
    assert!(output.status.success(), "{output:?}");
}

fn fixture() -> TemporaryDirectory {
    let directory = TemporaryDirectory::new();
    git(&directory.0, &["init", "--quiet"]);
    let jobserver = directory
        .0
        .join("local-app-data/NukeTheBees/jobserver/bin/jobserver.exe");
    fs::create_dir_all(jobserver.parent().unwrap()).unwrap();
    fs::write(jobserver, "installed tool marker").unwrap();
    fs::create_dir_all(directory.0.join("cmake/presets")).unwrap();
    fs::write(
        directory.0.join("cmake/presets/generate.py"),
        include_str!("fixtures/generate.py"),
    )
    .unwrap();
    fs::write(
        directory.0.join("CMakeLists.txt"),
        include_str!("fixtures/CMakeLists.txt"),
    )
    .unwrap();
    directory
}

fn tool(root: &Path, arguments: &[&str]) -> Command {
    let mut command = Command::new(env!("CARGO_BIN_EXE_agent-task"));
    command
        .args(arguments)
        .current_dir(root)
        .env("LOCALAPPDATA", root.join("local-app-data"));
    command
}

fn invoke(root: &Path, arguments: &[&str]) -> Output {
    tool(root, arguments).output().unwrap()
}

#[test]
fn help_and_invalid_arguments_do_not_start_initialization() {
    let directory = TemporaryDirectory::new();
    let version = invoke(&directory.0, &["--version"]);
    assert!(version.status.success());
    assert_eq!(
        String::from_utf8_lossy(&version.stdout).trim(),
        concat!("agent-task ", env!("CARGO_PKG_VERSION"))
    );
    let help = invoke(&directory.0, &["--help"]);
    assert!(help.status.success());
    let help = String::from_utf8_lossy(&help.stdout);
    assert!(help.contains("prepare-worktree"));
    assert!(help.contains("install-central-tools"));
    for arguments in [
        vec![],
        vec!["unknown"],
        vec!["start"],
        vec!["prepare-worktree", "--skip-build"],
        vec!["install-central-tools", "--all"],
    ] {
        let output = invoke(&directory.0, &arguments);
        assert_eq!(output.status.code(), Some(2));
        assert!(output.stdout.is_empty());
    }
}

#[test]
fn outside_worktree_fails_without_removing_out() {
    let directory = TemporaryDirectory::new();
    fs::create_dir(directory.0.join("out")).unwrap();
    for command in ["prepare-worktree", "install-central-tools"] {
        let output = invoke(&directory.0, &[command]);
        assert!(!output.status.success());
        assert!(String::from_utf8_lossy(&output.stderr).contains("run inside a Git worktree"));
        assert!(directory.0.join("out").is_dir());
    }
}

#[test]
fn nested_linked_worktree_cleans_only_its_root_out_and_runs_phases_in_order() {
    let directory = fixture();
    git(&directory.0, &["add", "."]);
    git(
        &directory.0,
        &[
            "-c",
            "user.name=Test",
            "-c",
            "user.email=test@example.invalid",
            "-c",
            "commit.gpgsign=false",
            "commit",
            "--quiet",
            "-m",
            "fixture",
        ],
    );
    git(
        &directory.0,
        &["worktree", "add", "--detach", "linked worktree"],
    );
    let root = directory.0.join("linked worktree");
    for path in [
        directory.0.join("out"),
        root.join("out"),
        root.join("nested/out"),
    ] {
        fs::create_dir_all(&path).unwrap();
        fs::write(path.join("keep.txt"), "original").unwrap();
    }
    fs::write(root.join("untracked.txt"), "source").unwrap();
    let settings = root.join("Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini");
    fs::create_dir_all(settings.parent().unwrap()).unwrap();
    fs::write(
        &settings,
        "[/Script/LiveCoding.LiveCodingSettings]\r\nbEnabled=True ; keep\r\n",
    )
    .unwrap();

    let output = tool(&root.join("nested"), &["prepare-worktree"])
        .env("LOCALAPPDATA", directory.0.join("local-app-data"))
        .output()
        .unwrap();
    assert!(output.status.success(), "{output:?}");
    assert!(!root.join("out/keep.txt").exists());
    assert_eq!(
        fs::read_to_string(settings).unwrap(),
        "[/Script/LiveCoding.LiveCodingSettings]\r\nbEnabled=False ; keep\r\n"
    );
    assert!(directory.0.join("out/keep.txt").is_file());
    assert!(root.join("nested/out/keep.txt").is_file());
    assert_eq!(
        fs::read_to_string(root.join("untracked.txt")).unwrap(),
        "source"
    );
    assert_eq!(
        fs::read_to_string(root.join("phases.txt"))
            .unwrap()
            .lines()
            .collect::<Vec<_>>(),
        ["presets", "generate-code"]
    );
    let stdout = String::from_utf8_lossy(&output.stdout);
    for phase in 1..=6 {
        assert!(stdout.contains(&format!("[{phase}/6]")), "{stdout}");
    }
    assert!(!stdout.contains("task-start"), "{stdout}");
}

#[test]
fn missing_out_and_saved_settings_are_allowed() {
    let directory = fixture();
    let output = invoke(&directory.0, &["prepare-worktree"]);
    assert!(output.status.success(), "{output:?}");
    assert!(!directory.0.join("Saved").exists());
}

#[test]
fn missing_central_tools_leave_build_output_untouched() {
    let directory = fixture();
    fs::create_dir(directory.0.join("out")).unwrap();
    fs::write(directory.0.join("out/keep.txt"), "build output").unwrap();
    let missing_installation = directory.0.join("empty-local-app-data");

    let mut missing_jobserver = tool(&directory.0, &["prepare-worktree"]);
    missing_jobserver.env("LOCALAPPDATA", &missing_installation);
    let mut missing_environment = tool(&directory.0, &["prepare-worktree"]);
    missing_environment.env_remove("LOCALAPPDATA");

    for mut command in [missing_jobserver, missing_environment] {
        let output = command.output().unwrap();
        assert!(!output.status.success());
        assert!(
            String::from_utf8_lossy(&output.stderr).contains("agent-task install-central-tools")
        );
        assert!(output.stdout.is_empty());
        assert_eq!(
            fs::read_to_string(directory.0.join("out/keep.txt")).unwrap(),
            "build output"
        );
        assert!(!directory.0.join("phases.txt").exists());
        assert!(!missing_installation.exists());
    }
}

#[test]
fn central_tool_installation_initializes_submodules_without_jobserver_or_clearing_out() {
    let directory = fixture();
    let dependency = TemporaryDirectory::new();
    git(&dependency.0, &["init", "--quiet"]);
    fs::write(dependency.0.join("marker.txt"), "dependency").unwrap();
    git(&dependency.0, &["add", "."]);
    git(
        &dependency.0,
        &[
            "-c",
            "user.name=Test",
            "-c",
            "user.email=test@example.invalid",
            "-c",
            "commit.gpgsign=false",
            "commit",
            "--quiet",
            "-m",
            "dependency",
        ],
    );
    git(
        &directory.0,
        &[
            "-c",
            "protocol.file.allow=always",
            "submodule",
            "add",
            "--",
            dependency.0.to_str().unwrap(),
            "dependency",
        ],
    );
    git(&directory.0, &["submodule", "deinit", "--force", "--all"]);
    assert!(!directory.0.join("dependency/marker.txt").exists());

    fs::create_dir(directory.0.join("out")).unwrap();
    fs::write(directory.0.join("out/keep.txt"), "build output").unwrap();
    fs::write(
        directory.0.join("cmake/presets/generate.py"),
        "from pathlib import Path\nassert Path('dependency/marker.txt').is_file()\nraise SystemExit(23)\n",
    )
    .unwrap();
    let output = tool(&directory.0, &["install-central-tools"])
        .env("LOCALAPPDATA", directory.0.join("empty-local-app-data"))
        .output()
        .unwrap();

    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("python failed"));
    assert!(String::from_utf8_lossy(&output.stderr).contains("23"));
    assert!(directory.0.join("dependency/marker.txt").is_file());
    let stdout = String::from_utf8_lossy(&output.stdout);
    assert!(stdout.contains("[1/7] Synchronizing submodules"));
    assert!(stdout.contains("[2/7] Updating submodules"));
    assert!(stdout.contains("[3/7] Generating CMake presets"));
    assert!(!stdout.contains("[4/7]"));
    assert_eq!(
        fs::read_to_string(directory.0.join("out/keep.txt")).unwrap(),
        "build output"
    );
}

#[test]
fn failed_cleanup_stops_before_submodules() {
    let directory = fixture();
    fs::write(directory.0.join("out"), "not a directory").unwrap();
    let output = invoke(&directory.0, &["prepare-worktree"]);
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("Could not remove"));
    assert!(!String::from_utf8_lossy(&output.stdout).contains("[2/6]"));
    assert!(directory.0.join("out").is_file());
}

#[test]
fn failed_preset_generation_stops_before_cmake() {
    let directory = fixture();
    fs::write(
        directory.0.join("cmake/presets/generate.py"),
        "raise SystemExit(23)\n",
    )
    .unwrap();
    let output = invoke(&directory.0, &["prepare-worktree"]);
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("python failed"));
    assert!(!String::from_utf8_lossy(&output.stdout).contains("[6/6]"));
    assert!(!directory.0.join("phases.txt").exists());
}

#[test]
fn failed_code_generation_returns_failure() {
    let directory = fixture();
    fs::write(
        directory.0.join("CMakeLists.txt"),
        "message(FATAL_ERROR \"fixture failure\")\n",
    )
    .unwrap();
    let output = invoke(&directory.0, &["prepare-worktree"]);
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("cmake failed"));
    assert!(String::from_utf8_lossy(&output.stdout).contains("[6/6]"));
    assert_eq!(
        fs::read_to_string(directory.0.join("phases.txt")).unwrap(),
        "presets\n"
    );
}
