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
            "set-live-coding-disabled-install-{}-{sequence}",
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

fn install_script() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .ancestors()
        .nth(2)
        .expect("workspace root should exist")
        .join("install/Install-SetLiveCodingDisabled.ps1")
}

fn installed_tool(root: &std::path::Path) -> PathBuf {
    root.join("bin/set-live-coding-disabled.exe")
}

fn run_installer(source: &std::path::Path, root: &std::path::Path) {
    let output = Command::new("pwsh")
        .args([
            "-NoProfile",
            "-File",
            install_script()
                .to_str()
                .expect("script path should be UTF-8"),
            "-BuiltToolPath",
            source.to_str().expect("source path should be UTF-8"),
            "-InstallRoot",
            root.to_str().expect("install root should be UTF-8"),
        ])
        .output()
        .expect("installer should run");
    assert!(
        output.status.success(),
        "installer failed:\n{}\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
}

#[test]
fn install_and_update_activate_the_staged_executable_safely() {
    let directory = TemporaryDirectory::new();
    let root = directory.path.join("NukeTheBees");
    let source = PathBuf::from(env!("CARGO_BIN_EXE_set-live-coding-disabled"));
    let source_bytes = fs::read(&source).expect("source executable should be read");

    run_installer(&source, &root);

    let installed = installed_tool(&root);
    assert_eq!(
        fs::read(&installed).expect("installed executable should be read"),
        source_bytes
    );
    let smoke = Command::new(&installed)
        .args([
            "--settings-path",
            root.join("missing.ini")
                .to_str()
                .expect("path should be UTF-8"),
        ])
        .status()
        .expect("installed executable should run");
    assert!(smoke.success());

    let mut old_bytes = source_bytes.clone();
    old_bytes.extend_from_slice(b"test-only previous executable marker");
    fs::write(&installed, &old_bytes)
        .expect("existing executable should be replaced for update test");

    run_installer(&source, &root);

    assert_eq!(
        fs::read(&installed).expect("replacement executable should be read"),
        source_bytes
    );
    assert_eq!(
        fs::read(root.join("previous/set-live-coding-disabled.exe"))
            .expect("previous executable should be retained"),
        old_bytes
    );
}
