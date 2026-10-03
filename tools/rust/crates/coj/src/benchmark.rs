use std::ffi::OsString;
use std::process::Command;

pub fn run(arguments: &[OsString]) -> Result<i32, String> {
    let root = crate::worktree_root()?;
    crate::run(&root, "cmake", &["--preset", "native"])?;
    crate::run(
        &root,
        "cmake",
        &[
            "--build",
            "--preset",
            "native",
            "--target",
            "benchmark-tools-host",
        ],
    )?;
    let executable = root
        .join("out/build/native/rust-tools/release")
        .join(if cfg!(windows) {
            "benchmark-tools.exe"
        } else {
            "benchmark-tools"
        });
    Command::new(executable)
        .args(arguments)
        .current_dir(root)
        .status()
        .map(|status| status.code().unwrap_or(1))
        .map_err(|error| format!("Cannot launch benchmark-tools: {error}"))
}
