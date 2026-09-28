//! Contracts against an isolated real daemon. No scheduling sleeps or polling.
use agent_scheduler::{Invocation, Scheduler};
use anyhow::{Result, ensure};
use std::{sync::Arc, time::Duration};
use tokio::process::Command;
use tokio_util::sync::CancellationToken;

async fn ticket(client: &Scheduler, mode: &str) -> Result<serde_json::Value> {
    client
        .request_ticket(mode, "lease-example", &CancellationToken::new())
        .await
}

async fn short_command(client: &Arc<Scheduler>) -> Result<()> {
    let invocation = Invocation::default();
    let observer = invocation
        .before_spawn(client, "cmd /c exit 0", &CancellationToken::new())
        .await?;
    let mut child = Command::new("cmd.exe").args(["/c", "exit", "0"]).spawn()?;
    invocation.accept();
    let status = child.wait().await?;
    observer.root_exited(status.code());
    ensure!(status.success());
    Ok(())
}

#[tokio::main]
async fn main() -> Result<()> {
    let rules = std::env::args().nth(1).expect("scheduling.rules path");
    let a = Scheduler::open(&rules).await?;
    let b = Scheduler::open(&rules).await?;
    let c = Scheduler::open(&rules).await?;
    let cancel = CancellationToken::new();
    let invocation = Invocation::default();
    invocation.before_spawn(&a, "rg --version", &cancel).await?;
    ensure!(a.status()?["ticket"] == false);
    ensure!(
        invocation
            .before_spawn(&a, "Write-Output NO_TICKET", &cancel)
            .await
            .is_err()
    );
    println!("PASS exempt inspection and missing-ticket rejection");

    ensure!(ticket(&a, "shared").await?["state"] == "granted");
    ensure!(ticket(&b, "shared").await?["state"] == "granted");
    ensure!(ticket(&c, "exclusive").await?["state"] == "queued");
    short_command(&a).await?;
    ensure!(ticket(&a, "shared").await?["state"] == "queued");
    ensure!(c.status()?["state"] == "queued");
    short_command(&b).await?;
    let exclusive = Invocation::default();
    exclusive.before_spawn(&c, "benchmark", &cancel).await?;
    ensure!(a.status()?["state"] == "queued");
    drop(exclusive);
    tokio::time::timeout(Duration::from_secs(3), short_command(&a)).await??;
    println!("PASS shared/shared, exclusive drain, FIFO barrier, pre-spawn release");

    for _ in 0..40 {
        ticket(&a, "shared").await?;
        short_command(&a).await?;
    }
    println!("PASS 40 immediate ticket/command cycles without sleeps or status polling");

    for race in [false, true] {
        for _ in 0..20 {
            ticket(&a, "shared").await?;
            ticket(&b, "exclusive").await?;
            let cancelled = CancellationToken::new();
            let pending = Invocation::default();
            {
                let wait = pending.before_spawn(&b, "must never launch", &cancelled);
                tokio::pin!(wait);
                // Claim the queued request without relying on elapsed time.
                tokio::select! { biased; result = &mut wait => panic!("unexpected admission: {result:?}"), _ = std::future::ready(()) => {} }
                if race {
                    short_command(&a).await?;
                }
                cancelled.cancel();
                ensure!(wait.await.is_err());
            }
            drop(pending);
            if !race {
                short_command(&a).await?;
            }
            ticket(&b, "shared").await?;
            short_command(&b).await?;
        }
    }
    println!("PASS queued cancellation, grant racing cancellation, immediate reuse");

    ticket(&a, "shared").await?;
    let before_spawn = Invocation::default();
    let cancelled = CancellationToken::new();
    cancelled.cancel();
    ensure!(
        before_spawn
            .before_spawn(&a, "cancel before spawn", &cancelled)
            .await
            .is_err()
    );
    drop(before_spawn);

    ticket(&a, "shared").await?;
    {
        let failed = Invocation::default();
        failed.before_spawn(&a, "missing program", &cancel).await?;
        ensure!(
            Command::new("scheduler-no-such-program.exe")
                .spawn()
                .is_err()
        );
    }
    ticket(&a, "exclusive").await?;
    short_command(&a).await?;
    println!("PASS actual spawn error relinquishes ticket");

    // Supplied only by the runner that owns this isolated daemon.
    let pid: u32 = std::env::var("SCHEDULER_EXAMPLE_DAEMON_PID")?.parse()?;
    ticket(&a, "shared").await?;
    let running = Invocation::default();
    let observer = running.before_spawn(&a, "already running", &cancel).await?;
    let mut child = Command::new("cmd.exe")
        .args(["/c", "pause"])
        .stdin(std::process::Stdio::piped())
        .kill_on_drop(true)
        .spawn()?;
    running.accept();
    ticket(&b, "exclusive").await?;
    let waiting = Invocation::default();
    let wait = waiting.before_spawn(&b, "blocked", &cancel);
    tokio::pin!(wait);
    tokio::select! { biased; result = &mut wait => panic!("unexpected admission: {result:?}"), _ = std::future::ready(()) => {} }
    unsafe {
        use windows_sys::Win32::System::Threading::{
            OpenProcess, PROCESS_TERMINATE, TerminateProcess,
        };
        let handle = OpenProcess(PROCESS_TERMINATE, 0, pid);
        ensure!(!handle.is_null());
        let result = TerminateProcess(handle, 0);
        windows_sys::Win32::Foundation::CloseHandle(handle);
        ensure!(result != 0);
    }
    ensure!(
        tokio::time::timeout(Duration::from_secs(3), wait)
            .await?
            .is_err()
    );
    tokio::time::timeout(Duration::from_secs(3), a.lost().cancelled()).await?;
    ensure!(
        child.try_wait()?.is_none(),
        "scheduler killed an already running process"
    );
    ensure!(ticket(&a, "shared").await.is_err());
    ensure!(
        Invocation::default()
            .before_spawn(&a, "rg --version", &cancel)
            .await
            .is_err()
    );
    child.kill().await?;
    observer.root_exited(child.wait().await?.code());
    println!("PASS disconnect wakes queue, fails future work, leaves running process to executor");
    Ok(())
}
