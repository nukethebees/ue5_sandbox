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

fn invoke(root: &Path, arguments: &[&str]) -> Output {
    Command::new(env!("CARGO_BIN_EXE_agent-task"))
        .args(arguments)
        .current_dir(root)
        .output()
        .unwrap()
}

#[test]
fn help_and_invalid_arguments_do_not_start_initialization() {
    let directory = TemporaryDirectory::new();
    assert!(invoke(&directory.0, &["--help"]).status.success());
    for arguments in [vec![], vec!["unknown"], vec!["start", "--skip-build"]] {
        let output = invoke(&directory.0, &arguments);
        assert_eq!(output.status.code(), Some(2));
        assert!(output.stdout.is_empty());
    }
}

#[test]
fn outside_worktree_fails_without_removing_out() {
    let directory = TemporaryDirectory::new();
    fs::create_dir(directory.0.join("out")).unwrap();
    let output = invoke(&directory.0, &["start"]);
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("run inside a Git worktree"));
    assert!(directory.0.join("out").is_dir());
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

    let output = invoke(&root.join("nested"), &["start"]);
    assert!(output.status.success(), "{output:?}");
    assert!(!root.join("out/keep.txt").exists());
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
        ["presets", "generate-code", "task-start"]
    );
    let stdout = String::from_utf8_lossy(&output.stdout);
    for phase in 1..=6 {
        assert!(stdout.contains(&format!("[{phase}/6]")), "{stdout}");
    }
}

#[test]
fn missing_out_is_allowed() {
    let directory = fixture();
    let output = invoke(&directory.0, &["start"]);
    assert!(output.status.success(), "{output:?}");
}

#[test]
fn failed_cleanup_stops_before_submodules() {
    let directory = fixture();
    fs::write(directory.0.join("out"), "not a directory").unwrap();
    let output = invoke(&directory.0, &["start"]);
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
    let output = invoke(&directory.0, &["start"]);
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("python failed"));
    assert!(!String::from_utf8_lossy(&output.stdout).contains("[5/6]"));
    assert!(!directory.0.join("phases.txt").exists());
}

#[test]
fn failed_code_generation_stops_before_baseline_build() {
    let directory = fixture();
    fs::write(
        directory.0.join("CMakeLists.txt"),
        "message(FATAL_ERROR \"fixture failure\")\n",
    )
    .unwrap();
    let output = invoke(&directory.0, &["start"]);
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("cmake failed"));
    assert!(!String::from_utf8_lossy(&output.stdout).contains("[6/6]"));
    assert_eq!(
        fs::read_to_string(directory.0.join("phases.txt")).unwrap(),
        "presets\n"
    );
}
