//! Executable demonstration against the real daemon; no model/API calls.
use agent_scheduler::Scheduler;
use anyhow::{Result, ensure};
use std::process::Stdio;
use std::time::Duration;
use tokio::io::{AsyncBufReadExt, BufReader};
use tokio::process::Command;
use tokio::sync::oneshot;
use tokio_util::sync::CancellationToken;

async fn ticket(client: &Scheduler, mode: &str) -> Result<()> {
    let executable = std::env::current_exe()?
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .join("agent-scheduler.exe");
    let output = Command::new(executable)
        .args(["ticket", mode, "lease-example"])
        .env("AGENT_SCHEDULER_SESSION", client.endpoint())
        .output()
        .await?;
    ensure!(
        output.status.success(),
        "ticket failed: {}",
        String::from_utf8_lossy(&output.stderr)
    );
    Ok(())
}

async fn idle(client: &Scheduler) -> Result<()> {
    let executable = std::env::current_exe()?
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .join("agent-scheduler.exe");
    for _ in 0..50 {
        let output = Command::new(&executable)
            .arg("status")
            .env("AGENT_SCHEDULER_SESSION", client.endpoint())
            .output()
            .await?;
        let status: serde_json::Value = serde_json::from_slice(&output.stdout)?;
        if status["ticket"] == false {
            return Ok(());
        }
        tokio::time::sleep(Duration::from_millis(20)).await;
    }
    anyhow::bail!("release was not acknowledged");
}

#[tokio::main]
async fn main() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();
    if args.get(1).map(String::as_str) == Some("child") {
        tokio::time::sleep(Duration::from_secs(4)).await;
        return Ok(());
    }
    if args.get(1).map(String::as_str) == Some("root") {
        let child = std::process::Command::new(&args[0])
            .arg("child")
            .stdin(Stdio::null())
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .spawn()?;
        println!("{}", child.id());
        return Ok(());
    }
    let rules = args.get(1).expect("scheduling.rules path");
    let a = Scheduler::open(rules).await?;
    let b = Scheduler::open(rules).await?;
    let c = Scheduler::open(rules).await?;
    let cancellation = CancellationToken::new();
    ensure!(
        a.before_spawn("rg --version", &cancellation)
            .await?
            .is_none()
    );
    ensure!(
        a.before_spawn("Write-Output NO_TICKET", &cancellation)
            .await
            .is_err()
    );
    println!("PASS exemption and missing-ticket rejection");

    ticket(&a, "shared").await?;
    let mut shared = a
        .before_spawn("cmake --build example", &cancellation)
        .await?
        .unwrap();
    shared.started();
    ticket(&b, "exclusive").await?;
    let waiting = tokio::spawn({
        let b = b.clone();
        async move { b.before_spawn("benchmark", &CancellationToken::new()).await }
    });
    ticket(&c, "shared").await?;
    let later = tokio::spawn({
        let c = c.clone();
        async move {
            c.before_spawn("later build", &CancellationToken::new())
                .await
        }
    });
    tokio::time::sleep(Duration::from_millis(150)).await;
    ensure!(!waiting.is_finished() && !later.is_finished());
    shared.finish(0);
    let mut exclusive = tokio::time::timeout(Duration::from_secs(2), waiting)
        .await???
        .unwrap();
    exclusive.started();
    ensure!(!later.is_finished());
    exclusive.finish(0);
    let later = tokio::time::timeout(Duration::from_secs(2), later)
        .await???
        .unwrap();
    drop(later);
    idle(&a).await?;
    idle(&b).await?;
    idle(&c).await?;
    println!("PASS exclusive drain and FIFO barrier for later shared work");

    ticket(&a, "shared").await?;
    let mut held = a
        .before_spawn("held command", &cancellation)
        .await?
        .unwrap();
    held.started();
    ticket(&b, "exclusive").await?;
    let cancel = CancellationToken::new();
    let pending = tokio::spawn({
        let b = b.clone();
        let cancel = cancel.clone();
        async move { b.before_spawn("must never launch", &cancel).await }
    });
    tokio::time::sleep(Duration::from_millis(50)).await;
    cancel.cancel();
    ensure!(pending.await?.is_err());
    held.finish(0);
    idle(&a).await?;
    idle(&b).await?;
    println!("PASS queued cancellation");

    ticket(&a, "shared").await?;
    let permit = a
        .before_spawn("persistent-descendant example", &cancellation)
        .await?
        .unwrap();
    let mut root = Command::new(&args[0])
        .arg("root")
        .stdout(Stdio::piped())
        .spawn()?;
    let (exit_sender, exit_receiver) = oneshot::channel();
    let done = permit.on_root_exit(exit_receiver);
    let mut pid_line = String::new();
    BufReader::new(root.stdout.take().unwrap())
        .read_line(&mut pid_line)
        .await?;
    let status = root.wait().await?;
    let child_pid: u32 = pid_line.trim().parse()?;
    exit_sender.send(status.code().unwrap_or(-1)).unwrap();
    ensure!(done.await? == 0);
    ticket(&b, "exclusive").await?;
    let exclusive = tokio::time::timeout(
        Duration::from_secs(2),
        b.before_spawn("benchmark after root", &cancellation),
    )
    .await??
    .unwrap();
    unsafe {
        use windows_sys::Win32::System::Threading::{
            GetExitCodeProcess, OpenProcess, PROCESS_QUERY_LIMITED_INFORMATION,
        };
        let child = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, child_pid);
        ensure!(!child.is_null(), "descendant was not alive");
        let mut code = 0;
        let queried = GetExitCodeProcess(child, &mut code);
        windows_sys::Win32::Foundation::CloseHandle(child);
        ensure!(
            queried != 0 && code == 259,
            "descendant exited before exclusive admission"
        );
    }
    drop(exclusive);
    idle(&b).await?;
    println!("PASS exclusive admitted after root exit while descendant is still alive");
    Ok(())
}
