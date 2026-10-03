use std::fs;
use std::path::Path;
use std::process::{Command, Stdio};
use std::thread::sleep;
use std::time::Duration;

fn idle() {
    loop {
        sleep(Duration::from_millis(20));
    }
}

fn main() {
    let args = std::env::args().skip(1).collect::<Vec<_>>();
    if args.first().is_some_and(|arg| arg == "idle") {
        idle();
    }
    if args.first().is_some_and(|arg| arg == "worker") {
        let _console = Command::new(
            std::env::current_exe()
                .unwrap()
                .parent()
                .unwrap()
                .join("conhost.exe"),
        )
        .arg("idle")
        .spawn()
        .unwrap();
        let child = Command::new(std::env::current_exe().unwrap())
            .arg("idle")
            .spawn()
            .unwrap();
        let path = Path::new(&args[1]);
        let temporary = path.with_extension("tmp");
        fs::write(&temporary, child.id().to_string()).unwrap();
        fs::rename(temporary, path).unwrap();
        idle();
    }
    assert_eq!(args, ["--no-daemon"]);
    let root = std::env::current_dir().unwrap();
    assert_eq!(std::env::var("COJ_CODEX_NAME").unwrap(), "test");
    assert_eq!(
        Path::new(&std::env::var_os("COJ_CODEX_WORKTREE").unwrap()).canonicalize().unwrap(),
        root.canonicalize().unwrap()
    );
    let marker = root.join("probe");
    fs::create_dir_all(&marker).unwrap();
    let runtime = std::env::current_exe()
        .unwrap()
        .parent()
        .unwrap()
        .to_owned();
    let helper = Command::new(runtime.join("codex-code-mode-host.exe"))
        .arg("idle")
        .spawn()
        .unwrap();
    let console = Command::new(root.join("work/conhost.exe"))
        .arg("idle")
        .spawn()
        .unwrap();
    let runner = Command::new(root.join(".sandbox-bin/codex-command-runner-test.exe"))
        .arg("idle")
        .spawn()
        .unwrap();
    let worker = Command::new(root.join("work/worker.exe"))
        .arg("worker")
        .arg(marker.join("grandchild"))
        .spawn()
        .unwrap();
    fs::write(
        marker.join("pids"),
        format!(
            "{}\n{}\n{}\n{}\n{}",
            std::process::id(),
            helper.id(),
            worker.id(),
            console.id(),
            runner.id()
        ),
    )
    .unwrap();
    while !marker.join("grandchild").exists() {
        sleep(Duration::from_millis(20));
    }
    fs::write(marker.join("ready"), "ready").unwrap();
    loop {
        let request = marker.join("request");
        if let Ok(text) = fs::read_to_string(&request) {
            fs::remove_file(&request).unwrap();
            if text == "exit" {
                return;
            }
            let output = Command::new(std::env::var_os("PROBE_COJ").unwrap())
                .arg("codex")
                .args(text.split_whitespace())
                .current_dir(&root)
                .stdin(Stdio::null())
                .output()
                .unwrap();
            let result = format!(
                "{}\n{}\n{}",
                output.status.code().unwrap_or(-1),
                String::from_utf8_lossy(&output.stdout),
                String::from_utf8_lossy(&output.stderr)
            );
            let temporary = marker.join("result.tmp");
            fs::write(&temporary, result).unwrap();
            fs::rename(temporary, marker.join("result")).unwrap();
        }
        sleep(Duration::from_millis(20));
    }
}
