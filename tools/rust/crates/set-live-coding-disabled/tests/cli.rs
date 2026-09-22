use std::fs;
use std::path::PathBuf;
use std::process::Command;
use std::sync::atomic::{AtomicUsize, Ordering};

static NEXT_DIRECTORY: AtomicUsize = AtomicUsize::new(0);

struct TemporaryDirectory {
    path: PathBuf,
}

impl TemporaryDirectory {
    fn new() -> Self {
        let sequence = NEXT_DIRECTORY.fetch_add(1, Ordering::Relaxed);
        let path = std::env::temp_dir().join(format!(
            "set-live-coding-disabled-cli-{}-{sequence}",
            std::process::id()
        ));
        fs::create_dir(&path).expect("temporary directory should be created");
        Self { path }
    }
}

impl Drop for TemporaryDirectory {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.path);
    }
}

fn tool() -> Command {
    Command::new(env!("CARGO_BIN_EXE_set-live-coding-disabled"))
}

#[test]
fn invalid_arguments_return_usage_error() {
    let output = tool().output().expect("tool should run");

    assert_eq!(output.status.code(), Some(2));
    assert_eq!(
        String::from_utf8(output.stderr).expect("stderr should be UTF-8"),
        "Usage: set-live-coding-disabled --settings-path <path>\n"
    );
}

#[test]
fn missing_file_succeeds_without_creating_it() {
    let directory = TemporaryDirectory::new();
    let settings_path = directory.path.join("Missing.ini");
    let output = tool()
        .args([
            "--settings-path",
            settings_path.to_str().expect("path should be UTF-8"),
        ])
        .output()
        .expect("tool should run");

    assert_eq!(output.status.code(), Some(0));
    assert!(output.stdout.is_empty());
    assert!(!settings_path.exists());
}

#[test]
fn successful_change_reports_absolute_path() {
    let directory = TemporaryDirectory::new();
    let settings_path = directory.path.join("Settings.ini");
    fs::write(
        &settings_path,
        "[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=True\n",
    )
    .expect("input should be written");
    let expected_path = std::path::absolute(&settings_path).expect("path should resolve");

    let output = tool()
        .args([
            "--settings-path",
            settings_path.to_str().expect("path should be UTF-8"),
        ])
        .output()
        .expect("tool should run");

    assert_eq!(output.status.code(), Some(0));
    assert_eq!(
        String::from_utf8(output.stdout).expect("stdout should be UTF-8"),
        format!("Disabled Live Coding in '{}'.\n", expected_path.display())
    );
}

#[test]
fn invalid_encoding_returns_runtime_error() {
    let directory = TemporaryDirectory::new();
    let settings_path = directory.path.join("Settings.ini");
    fs::write(&settings_path, [0xff]).expect("input should be written");

    let output = tool()
        .args([
            "--settings-path",
            settings_path.to_str().expect("path should be UTF-8"),
        ])
        .output()
        .expect("tool should run");

    assert_eq!(output.status.code(), Some(1));
    assert!(
        String::from_utf8(output.stderr)
            .expect("stderr should be UTF-8")
            .starts_with("set-live-coding-disabled: Invalid UTF-8 input.")
    );
}
