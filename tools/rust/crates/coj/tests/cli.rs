use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Command, Output};
use std::sync::atomic::{AtomicUsize, Ordering};

static NEXT_DIRECTORY: AtomicUsize = AtomicUsize::new(0);

struct TemporaryDirectory(PathBuf);

impl TemporaryDirectory {
    fn new() -> Self {
        let sequence = NEXT_DIRECTORY.fetch_add(1, Ordering::Relaxed);
        let path = ioj_test_support::temp_root()
            .join(format!("coj test {}-{sequence}", std::process::id()));
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

fn tool(root: &Path, arguments: &[&str]) -> Command {
    let mut command = Command::new(env!("CARGO_BIN_EXE_coj"));
    command.args(arguments).current_dir(root);
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
        concat!("coj ", env!("CARGO_PKG_VERSION"))
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
fn jobs_help_and_invalid_syntax_work_without_an_external_client() {
    let directory = TemporaryDirectory::new();
    let help = tool(&directory.0, &["jobs", "--help"])
        .env("PATH", directory.0.join("missing-tools"))
        .output()
        .unwrap();
    assert!(help.status.success());
    let text = String::from_utf8_lossy(&help.stdout);
    for command in [
        "request", "check", "start", "end", "cancel", "clear", "status",
    ] {
        assert!(text.contains(command), "{text}");
    }
    for arguments in [
        vec!["jobs"],
        vec!["jobs", "unknown"],
        vec!["jobs", "jobs", "end", "85"],
        vec!["jobs", "request", "other", "name"],
        vec!["jobs", "request", "shared", ""],
        vec!["jobs", "request", "shared"],
        vec!["jobs", "check", "0"],
        vec!["jobs", "start", "-1"],
        vec!["jobs", "end", "1x"],
        vec!["jobs", "cancel", "18446744073709551616"],
        vec!["jobs", "status", "extra"],
        vec!["jobs", "clear"],
        vec!["jobs", "clear", "85", "--owner", "dev5"],
        vec!["jobs", "clear", "--worktree", "."],
    ] {
        let output = invoke(&directory.0, &arguments);
        assert_eq!(output.status.code(), Some(2), "{arguments:?}: {output:?}");
        assert!(!output.stderr.is_empty(), "{arguments:?}: {output:?}");
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
    for phase in 1..=5 {
        assert!(stdout.contains(&format!("[{phase}/5]")), "{stdout}");
    }
}

#[test]
fn missing_out_and_saved_settings_are_allowed() {
    let directory = fixture();
    let output = invoke(&directory.0, &["prepare-worktree"]);
    assert!(output.status.success(), "{output:?}");
    assert!(!directory.0.join("Saved").exists());
}

#[test]
fn invalid_saved_settings_warn_and_allow_code_generation() {
    let directory = fixture();
    let settings = directory
        .0
        .join("Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini");
    fs::create_dir_all(settings.parent().unwrap()).unwrap();
    fs::write(&settings, [0xff]).unwrap();

    let output = invoke(&directory.0, &["prepare-worktree"]);
    assert!(output.status.success(), "{output:?}");
    assert!(
        String::from_utf8_lossy(&output.stderr).contains("warning: Could not disable Live Coding")
    );
    assert_eq!(
        fs::read_to_string(directory.0.join("phases.txt"))
            .unwrap()
            .lines()
            .collect::<Vec<_>>(),
        ["presets", "generate-code"]
    );
    assert_eq!(fs::read(settings).unwrap(), [0xff]);
}

#[test]
fn central_tool_installation_updates_submodules_and_installs_jobserver() {
    let directory = fixture();
    fs::create_dir(directory.0.join("out")).unwrap();
    fs::write(directory.0.join("out/keep.txt"), "build output").unwrap();
    fs::write(directory.0.join("CMakePresets.json"), r#"{
        "version": 6,
        "configurePresets": [{"name":"native", "generator":"Ninja", "binaryDir":"${sourceDir}/out/native", "cacheVariables":{"PHASE":"configure"}}],
        "buildPresets": [{"name":"native", "configurePreset":"native"}]
    }"#).unwrap();
    fs::write(directory.0.join("CMakeLists.txt"), concat!(
        "cmake_minimum_required(VERSION 3.25)\nproject(Fixture NONE)\n",
        "file(APPEND \"${CMAKE_SOURCE_DIR}/phases.txt\" \"configure\\n\")\n",
        "add_custom_target(install-jobserver COMMAND \"${CMAKE_COMMAND}\" -E touch \"${CMAKE_SOURCE_DIR}/installed.txt\")\n"
    )).unwrap();
    let output = invoke(&directory.0, &["install-central-tools"]);
    assert!(output.status.success(), "{output:?}");
    assert!(directory.0.join("installed.txt").is_file());
    assert_eq!(
        fs::read_to_string(directory.0.join("phases.txt")).unwrap(),
        if cfg!(windows) {
            "configure\r\n"
        } else {
            "configure\n"
        }
    );
    assert_eq!(
        fs::read_to_string(directory.0.join("out/keep.txt")).unwrap(),
        "build output"
    );
    let stdout = String::from_utf8_lossy(&output.stdout);
    let sync = stdout.find("Synchronizing submodules").unwrap();
    let update = stdout.find("Updating submodules").unwrap();
    let configure = stdout.find("Configuring native build").unwrap();
    let install = stdout.find("Installing canonical jobserver").unwrap();
    assert!(sync < update && update < configure && configure < install);
}

#[test]
fn failed_cleanup_stops_before_submodules() {
    let directory = fixture();
    fs::write(directory.0.join("out"), "not a directory").unwrap();
    let output = invoke(&directory.0, &["prepare-worktree"]);
    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("Could not remove"));
    assert!(!String::from_utf8_lossy(&output.stdout).contains("Synchronizing submodules"));
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
    assert!(!String::from_utf8_lossy(&output.stdout).contains("Generating code"));
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
    assert!(String::from_utf8_lossy(&output.stdout).contains("Generating code"));
    assert_eq!(
        fs::read_to_string(directory.0.join("phases.txt")).unwrap(),
        "presets\n"
    );
}
