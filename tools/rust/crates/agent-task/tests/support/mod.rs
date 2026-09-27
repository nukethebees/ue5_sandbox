#![allow(dead_code)]
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::atomic::{AtomicUsize, Ordering};

static NEXT: AtomicUsize = AtomicUsize::new(0);
pub struct Repo {
    pub root: PathBuf,
    pub dev: PathBuf,
    pub feature: PathBuf,
}
impl Repo {
    pub fn new() -> Self {
        let root = std::env::temp_dir().join(format!(
            "agent-task git {}-{}",
            std::process::id(),
            NEXT.fetch_add(1, Ordering::Relaxed)
        ));
        let dev = root.join("dev");
        let feature = root.join("dev1");
        fs::create_dir_all(&dev).unwrap();
        let repo = Self { root, dev, feature };
        repo.raw(&repo.dev, &["init", "-q", "-b", "dev"]);
        repo.raw(&repo.dev, &["config", "user.name", "Test"]);
        repo.raw(&repo.dev, &["config", "user.email", "test@example.invalid"]);
        repo.raw(&repo.dev, &["config", "core.autocrlf", "false"]);
        repo.raw(&repo.dev, &["config", "commit.gpgsign", "false"]);
        fs::write(repo.dev.join("file.txt"), "base\n").unwrap();
        repo.raw(&repo.dev, &["add", "."]);
        repo.raw(&repo.dev, &["commit", "-qm", "base"]);
        for name in ["main", "master", "dev1"] {
            repo.raw(&repo.dev, &["branch", name]);
        }
        repo.raw(
            &repo.dev,
            &[
                "worktree",
                "add",
                "-b",
                "feature",
                repo.feature.to_str().unwrap(),
            ],
        );
        repo
    }
    pub fn git(&self, cwd: &Path) -> Command {
        let mut cmd = Command::new("git");
        self.environment(&mut cmd);
        cmd.current_dir(cwd);
        cmd
    }
    pub fn environment(&self, cmd: &mut Command) {
        cmd.env("GIT_CONFIG_NOSYSTEM", "1")
            .env("GIT_CONFIG_GLOBAL", self.root.join("empty-config"))
            .env("GIT_TERMINAL_PROMPT", "0")
            .env("GIT_EDITOR", "true");
        cmd.env_remove("NUKETHEBEES_JOBSERVER_LEASE");
    }
    pub fn raw(&self, cwd: &Path, args: &[&str]) -> String {
        let output = self.git(cwd).args(args).output().unwrap();
        assert!(output.status.success(), "{args:?}: {output:?}");
        String::from_utf8(output.stdout)
            .unwrap()
            .trim_end()
            .to_owned()
    }
    pub fn tool(&self) -> Command {
        let mut cmd =
            Command::new(option_env!("CARGO_BIN_EXE_agent-task").expect("CLI test binary"));
        self.environment(&mut cmd);
        cmd.current_dir(&self.feature);
        cmd
    }
    pub fn ok(&self, args: &[&str]) {
        let output = self.tool().arg("git").args(args).output().unwrap();
        assert!(output.status.success(), "{args:?}: {output:?}");
    }
    pub fn blocked(&self, args: &[&str]) {
        let output = self.tool().arg("git").args(args).output().unwrap();
        assert!(!output.status.success(), "{args:?}");
        assert!(
            String::from_utf8_lossy(&output.stderr).contains("agent-task:"),
            "{args:?}: {output:?}"
        );
    }
}
impl Drop for Repo {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.root);
    }
}
