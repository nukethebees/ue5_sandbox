#![cfg(windows)]

use std::fs;
use std::os::windows::io::{AsRawHandle, FromRawHandle, OwnedHandle};
use std::os::windows::process::CommandExt;
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Output, Stdio};
use std::sync::{
    OnceLock,
    atomic::{AtomicUsize, Ordering},
};
use std::thread::sleep;
use std::time::{Duration, Instant};
use windows_sys::Win32::Foundation::CloseHandle;
use windows_sys::Win32::System::Threading::{
    OpenProcess, PROCESS_SYNCHRONIZE, PROCESS_TERMINATE, TerminateProcess, WaitForSingleObject,
};

const NO_WINDOW: u32 = 0x08000000;
static NEXT: AtomicUsize = AtomicUsize::new(0);

fn helper() -> &'static Path {
    static EXE: OnceLock<PathBuf> = OnceLock::new();
    EXE.get_or_init(|| {
        let directory = Path::new(env!("CARGO_TARGET_TMPDIR"))
            .join(format!("codex-helper-{}", std::process::id()));
        fs::create_dir_all(&directory).unwrap();
        let executable = directory.join("codex-probe.exe");
        let output = Command::new("rustc")
            .arg(Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/fixtures/codex_process.rs"))
            .arg("--edition=2024")
            .arg("-o")
            .arg(&executable)
            .creation_flags(NO_WINDOW)
            .output()
            .unwrap();
        assert!(
            output.status.success(),
            "{}",
            String::from_utf8_lossy(&output.stderr)
        );
        executable
    })
}

struct Fixture {
    root: PathBuf,
}

impl Fixture {
    fn new() -> Self {
        let root = std::env::temp_dir().join(format!(
            "agent-task codex {} {}",
            std::process::id(),
            NEXT.fetch_add(1, Ordering::Relaxed)
        ));
        for directory in [".git", "runtime", "work", ".sandbox-bin"] {
            fs::create_dir_all(root.join(directory)).unwrap();
        }
        for file in [
            "runtime/codex.exe",
            "runtime/codex-code-mode-host.exe",
            "work/worker.exe",
            "work/conhost.exe",
            ".sandbox-bin/codex-command-runner-test.exe",
        ] {
            fs::copy(helper(), root.join(file)).unwrap();
        }
        Self { root }
    }

    fn command(&self, args: &[&str]) -> Command {
        let mut command = Command::new(env!("CARGO_BIN_EXE_agent-task"));
        let mut paths = vec![self.root.join("runtime")];
        paths.extend(std::env::split_paths(&std::env::var_os("PATH").unwrap()));
        command
            .arg("codex")
            .args(args)
            .current_dir(&self.root)
            .env("IOJ_ROOT", self.root.join("shared"))
            .env("PATH", std::env::join_paths(paths).unwrap())
            .env("PROBE_AGENT_TASK", env!("CARGO_BIN_EXE_agent-task"))
            .creation_flags(NO_WINDOW);
        command
    }

    fn invoke(&self, args: &[&str]) -> Output {
        self.command(args).output().unwrap()
    }

    fn launch(&self) -> Running {
        let child = self
            .command(&["start", "test"])
            .stdin(Stdio::null())
            .stdout(Stdio::null())
            .spawn()
            .unwrap();
        let mut running = Running(child, Vec::new());
        wait_until(|| {
            assert!(
                running.0.try_wait().unwrap().is_none(),
                "launcher exited before the fixture became ready"
            );
            self.root.join("probe/ready").exists()
        });
        let listing = self.invoke(&["processes", "test"]);
        assert!(listing.status.success(), "{listing:?}");
        for line in String::from_utf8_lossy(&listing.stdout).lines() {
            let pid = line.split_whitespace().next().unwrap().parse().unwrap();
            let handle = unsafe { OpenProcess(PROCESS_TERMINATE | PROCESS_SYNCHRONIZE, 0, pid) };
            assert!(!handle.is_null(), "Open fixture process {pid}");
            running
                .1
                .push(unsafe { OwnedHandle::from_raw_handle(handle) });
        }
        running
    }

    fn pids(&self) -> Vec<u32> {
        let mut pids = fs::read_to_string(self.root.join("probe/pids"))
            .unwrap()
            .lines()
            .map(|v| v.parse().unwrap())
            .collect::<Vec<u32>>();
        pids.push(
            fs::read_to_string(self.root.join("probe/grandchild"))
                .unwrap()
                .parse()
                .unwrap(),
        );
        pids
    }

    fn request(&self, request: &str) -> String {
        let temporary = self.root.join("probe/request.tmp");
        fs::write(&temporary, request).unwrap();
        fs::rename(temporary, self.root.join("probe/request")).unwrap();
        let path = self.root.join("probe/result");
        wait_until(|| path.exists());
        let result = fs::read_to_string(&path).unwrap();
        fs::remove_file(path).unwrap();
        result
    }
}

impl Drop for Fixture {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.root);
    }
}

struct Running(Child, Vec<OwnedHandle>);
impl Drop for Running {
    fn drop(&mut self) {
        // Retain process handles so teardown also works after the launcher closes the job.
        for handle in &self.1 {
            unsafe {
                TerminateProcess(handle.as_raw_handle(), 130);
                WaitForSingleObject(handle.as_raw_handle(), 5000);
            }
        }
        let _ = self.0.kill();
        let _ = self.0.wait();
    }
}

fn wait_until(mut predicate: impl FnMut() -> bool) {
    let deadline = Instant::now() + Duration::from_secs(15);
    while !predicate() {
        assert!(
            Instant::now() < deadline,
            "timed out waiting for fixture process"
        );
        sleep(Duration::from_millis(20));
    }
}

fn alive(pid: u32) -> bool {
    unsafe {
        let handle = OpenProcess(PROCESS_SYNCHRONIZE, 0, pid);
        if handle.is_null() {
            return false;
        }
        let result = WaitForSingleObject(handle, 0) == 258;
        CloseHandle(handle);
        result
    }
}

#[test]
fn cleanup_preserves_runtime_and_only_terminates_owned_work() {
    let fixture = Fixture::new();
    let mut running = fixture.launch();
    let outsider = Running(
        Command::new(fixture.root.join("work/worker.exe"))
            .arg("idle")
            .creation_flags(NO_WINDOW)
            .spawn()
            .unwrap(),
        Vec::new(),
    );
    let pids = fixture.pids();
    let listing = fixture.invoke(&["processes", "test"]);
    assert!(listing.status.success(), "{listing:?}");
    for pid in &pids {
        assert!(String::from_utf8_lossy(&listing.stdout).contains(&pid.to_string()));
    }

    let rejected = fixture.invoke(&["clean", "test"]);
    assert!(!rejected.status.success());
    assert!(String::from_utf8_lossy(&rejected.stderr).contains("inside the named Codex job"));
    let dry_run = fixture.request("clean test --dry-run");
    assert!(dry_run.starts_with("0\n"), "{dry_run}");
    assert!(dry_run.contains("Selected 2 child processes"), "{dry_run}");
    assert!(pids.iter().all(|pid| alive(*pid)));

    let cleaned = fixture.request("clean test");
    assert!(cleaned.starts_with("0\n"), "{cleaned}");
    assert!(cleaned.contains("Stopped 2 child processes"), "{cleaned}");
    assert!(alive(pids[0]) && alive(pids[1]) && alive(outsider.0.id()));
    assert!(!alive(pids[2]) && !alive(pids[5]));
    assert!(alive(pids[3]) && alive(pids[4]));
    assert!(running.0.try_wait().unwrap().is_none());
    assert!(
        fixture
            .request("clean test")
            .contains("Stopped 0 child processes")
    );

    fs::write(fixture.root.join("probe/request"), "exit").unwrap();
    wait_until(|| running.0.try_wait().unwrap().is_some());
    assert!(running.0.wait().unwrap().success());
    assert!(!fixture.root.join(".local/codex/test.pid").exists());
}

#[test]
fn worktrees_and_session_names_are_isolated() {
    let first = Fixture::new();
    let _first_running = first.launch();
    let second = Fixture::new();
    let _second_running = second.launch();
    let duplicate = first.invoke(&["start", "test"]);
    assert!(!duplicate.status.success());
    assert!(String::from_utf8_lossy(&duplicate.stderr).contains("already running"));
    let other_name = first.request("clean other");
    assert!(other_name.contains("Open session job"), "{other_name}");
    assert!(first.request("clean test").starts_with("0\n"));
    assert!(second.pids().iter().all(|pid| alive(*pid)));
}

#[test]
fn invalid_names_and_extra_arguments_are_rejected() {
    let fixture = Fixture::new();
    for name in ["../other", "Upper", "", "a/b"] {
        assert!(!fixture.invoke(&["start", name]).status.success());
    }
    for args in [
        vec!["--remote", "ws://localhost:1"],
        vec!["--cd", ".."],
        vec!["--worktree"],
    ] {
        let mut command = vec!["start", "test", "--"];
        command.extend(args);
        assert!(!fixture.invoke(&command).status.success());
    }
    assert!(!fixture.root.join("probe").exists());
}
