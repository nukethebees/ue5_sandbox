use std::{env, fs, path::PathBuf};

pub use tempfile::TempDir;

/// Create the shared temporary directory for the checkout containing the current directory.
pub fn temp_root() -> PathBuf {
    let ioj_root = env::var_os("IOJ_ROOT")
        .map(PathBuf::from)
        .filter(|path| path.is_absolute())
        .expect("Set IOJ_ROOT to an absolute directory for shared tools and temporary files.");
    let cwd = env::current_dir().expect("Read the test working directory");
    let worktree = cwd
        .ancestors()
        .find(|path| path.join(".git").exists())
        .expect("Run Rust tests from inside the Git worktree");
    let name = worktree
        .file_name()
        .expect("Read the worktree directory name");
    let directory = ioj_root.join("tmp").join(name);
    fs::create_dir_all(&directory).expect("Create the worktree test temporary directory");
    directory
}

/// Create an isolated fixture beneath the worktree's temporary directory, cleaned up on drop.
pub fn temp_dir(prefix: &str) -> TempDir {
    tempfile::Builder::new()
        .prefix(prefix)
        .tempdir_in(temp_root())
        .expect("Create a test fixture directory")
}
