//! Real local unified-exec backends and approvals, driven through Codex's conversation API.
//! The command/exec app-server API is deliberately not used.
mod support;
use agent_scheduler::{Invocation, Scheduler};
use anyhow::{Context, Result, ensure};
use serde_json::{Value, json};
use std::{
    io::{Read, Write},
    path::Path,
    process::Stdio,
    time::Duration,
};
use tokio::io::{AsyncBufReadExt, AsyncReadExt, AsyncWriteExt, BufReader};
use tokio::net::{TcpListener, windows::named_pipe::ServerOptions};
use tokio::process::Command;
use tokio_util::sync::CancellationToken;

fn helper(args: &[String]) -> Result<bool> {
    match args.get(1).map(String::as_str) {
        Some("root") => {
            std::process::Command::new(&args[0])
                .args(["descendant", &args[2], &args[3]])
                .stdin(Stdio::null())
                .stdout(Stdio::inherit())
                .stderr(Stdio::inherit())
                .spawn()?;
        }
        Some("descendant") => {
            let mut pipe = std::fs::OpenOptions::new()
                .read(true)
                .write(true)
                .open(&args[2])?;
            writeln!(pipe, "{} {}", std::process::id(), args[3])?;
            // The inherited stdout/stderr remain open until the parent test explicitly releases us.
            let mut release = [0];
            pipe.read_exact(&mut release)?;
            eprintln!("DESCENDANT_FINISHED");
        }
        Some("retry-write") => {
            let result = std::fs::write(&args[3], "retry succeeded");
            let mut log = std::fs::OpenOptions::new()
                .create(true)
                .append(true)
                .open(&args[2])?;
            match result {
                Ok(()) => writeln!(log, "success")?,
                Err(error) => {
                    ensure!(
                        error.kind() == std::io::ErrorKind::PermissionDenied,
                        "unexpected write failure: {error}"
                    );
                    writeln!(log, "denied")?;
                    eprintln!("failed to write file: permission denied: {error}");
                    std::process::exit(1);
                }
            }
        }
        _ => return Ok(false),
    }
    Ok(true)
}

fn process_alive(pid: u32) -> Result<bool> {
    unsafe {
        use windows_sys::Win32::System::Threading::{
            GetExitCodeProcess, OpenProcess, PROCESS_QUERY_LIMITED_INFORMATION,
        };
        let handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, pid);
        if handle.is_null() {
            ensure!(
                windows_sys::Win32::Foundation::GetLastError()
                    == windows_sys::Win32::Foundation::ERROR_INVALID_PARAMETER,
                "cannot inspect process {pid}"
            );
            return Ok(false);
        }
        let mut code = 0;
        let queried = GetExitCodeProcess(handle, &mut code);
        windows_sys::Win32::Foundation::CloseHandle(handle);
        ensure!(queried != 0);
        Ok(code == 259)
    }
}

fn descendant_pipe(name: &str) -> Result<tokio::net::windows::named_pipe::NamedPipeServer> {
    // Test rendezvous only: a restricted child must be able to connect and acknowledge.
    unsafe {
        use windows_sys::Win32::Security::{
            Authorization::ConvertStringSecurityDescriptorToSecurityDescriptorW,
            SECURITY_ATTRIBUTES,
        };
        let sddl: Vec<u16> = "D:NO_ACCESS_CONTROLS:(ML;;NW;;;LW)\0"
            .encode_utf16()
            .collect();
        let mut descriptor = std::ptr::null_mut();
        ensure!(
            ConvertStringSecurityDescriptorToSecurityDescriptorW(
                sddl.as_ptr(),
                1,
                &mut descriptor,
                std::ptr::null_mut()
            ) != 0
        );
        let mut attributes = SECURITY_ATTRIBUTES {
            nLength: std::mem::size_of::<SECURITY_ATTRIBUTES>() as u32,
            lpSecurityDescriptor: descriptor,
            bInheritHandle: 0,
        };
        let pipe = ServerOptions::new()
            .first_pipe_instance(true)
            .create_with_security_attributes_raw(
                name,
                (&mut attributes as *mut SECURITY_ATTRIBUTES).cast(),
            );
        windows_sys::Win32::Foundation::LocalFree(descriptor);
        Ok(pipe?)
    }
}

async fn send(input: &mut tokio::process::ChildStdin, message: Value) -> Result<()> {
    input.write_all(format!("{message}\n").as_bytes()).await?;
    Ok(())
}

async fn run_codex(home: &Path, cwd: &Path, bin: &Path, rules: &Path) -> Result<Vec<Value>> {
    let mut child = Command::new(bin.join("codex.exe"))
        .args(["app-server", "--listen", "stdio://"])
        .env("CODEX_HOME", home)
        .env("AGENT_SCHEDULER_RULES", rules)
        .env(
            "PATH",
            format!(
                "{};{};{}",
                bin.display(),
                bin.join("examples").display(),
                std::env::var("PATH")?
            ),
        )
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::inherit())
        .kill_on_drop(true)
        .spawn()?;
    let mut input = child.stdin.take().unwrap();
    let mut lines = BufReader::new(child.stdout.take().unwrap()).lines();
    send(&mut input, json!({"id":1,"method":"initialize","params":{"clientInfo":{"name":"scheduler-regression","version":"0.1"},"capabilities":{"experimentalApi":true}}})).await?;
    let mut approvals = Vec::new();
    while let Some(line) = lines.next_line().await? {
        let message: Value = serde_json::from_str(&line).context(line)?;
        ensure!(message.get("error").is_none(), "Codex RPC error: {message}");
        if message["id"] == 1 && message.get("result").is_some() {
            send(&mut input, json!({"method":"initialized"})).await?;
            send(&mut input, json!({"id":2,"method":"thread/start","params":{"cwd":cwd,"sandbox":"workspace-write"}})).await?;
        } else if message["id"] == 2 && message.get("result").is_some() {
            send(&mut input, json!({"id":3,"method":"turn/start","params":{"threadId":message["result"]["thread"]["id"],"input":[{"type":"text","text":"Run the scripted scheduler regression."}]}})).await?;
        } else if message["method"] == "item/commandExecution/requestApproval" {
            let command = message["params"]["command"].as_str().unwrap_or("");
            ensure!(
                command.contains("codex_lifecycle") || command.contains("Write-Output"),
                "unexpected approval: {message}"
            );
            approvals.push(message.clone());
            send(
                &mut input,
                json!({"id":message["id"],"result":{"decision":"accept"}}),
            )
            .await?;
        } else if message["method"] == "turn/completed" {
            ensure!(
                message["params"]["turn"]["status"] == "completed",
                "turn failed: {message}"
            );
            child.kill().await?;
            child.wait().await?;
            return Ok(approvals);
        }
    }
    anyhow::bail!("Codex exited before turn completion")
}

fn configure(home: &Path, address: std::net::SocketAddr, sandbox: &str) -> Result<()> {
    std::fs::create_dir_all(home.join("rules"))?;
    std::fs::write(
        home.join("rules/default.rules"),
        "prefix_rule(pattern=[\"agent-scheduler\", [\"ticket\", \"status\", \"clear\"]], decision=\"allow\")\n",
    )?;
    std::fs::write(
        home.join("config.toml"),
        format!(
            r#"
model = "gpt-5.4"
model_provider = "fixture"
approval_policy = {{ granular = {{ sandbox_approval = true, rules = true, mcp_elicitations = false }} }}
sandbox_mode = "workspace-write"
[windows]
sandbox = "{sandbox}"
[features]
shell_snapshot = false
[model_providers.fixture]
name = "local fixture"
base_url = "http://{address}/v1"
wire_api = "responses"
requires_openai_auth = false
"#
        ),
    )?;
    Ok(())
}

async fn root_case(repo: &Path, sandbox: &str, tty: bool) -> Result<()> {
    let label = format!("{sandbox}-{tty}");
    let home = repo.join(format!(".local/scheduler-lifecycle/{label}"));
    let cwd = home.join("workspace");
    std::fs::create_dir_all(&cwd)?;
    let bin = repo.join(".local/scheduler-target/debug");
    let rules = repo.join("tools/agent-scheduler/scheduling.rules");
    let http = TcpListener::bind("127.0.0.1:0").await?;
    configure(&home, http.local_addr()?, "unelevated")?;
    let pipe_name = format!(
        r"\\.\pipe\SchedulerDescendant.{}.{label}",
        std::process::id()
    );
    let mut pipe = descendant_pipe(&pipe_name)?;
    let exclusive = Scheduler::open(rules.to_str().unwrap()).await?;
    let console_teardown = sandbox == "unelevated" && tty;
    let helper = std::env::current_exe()?;
    let command = format!("& '{}' root '{pipe_name}' $PID", helper.display());
    let mut root_command = json!({"cmd":command,"login":false,"yield_time_ms":1000,"tty":tty});
    if sandbox == "none" {
        root_command["sandbox_permissions"] = json!("require_escalated");
    }
    let commands = [
        json!({"cmd":"agent-scheduler ticket shared root-test","login":false}),
        root_command,
    ];
    let fixture = tokio::spawn(async move {
        let mut requests = Vec::new();
        for index in 0..=commands.len() {
            let (socket, request) = support::request(&http).await?;
            requests.push(request);
            if index == commands.len() {
                tokio::time::timeout(Duration::from_secs(3), pipe.connect())
                    .await
                    .context("descendant did not connect")??;
                let mut line = String::new();
                BufReader::new(&mut pipe).read_line(&mut line).await?;
                let ids = line
                    .split_whitespace()
                    .map(str::parse::<u32>)
                    .collect::<std::result::Result<Vec<_>, _>>()?;
                ensure!(ids.len() == 2);
                exclusive
                    .request_ticket("exclusive", "after-root", &CancellationToken::new())
                    .await?;
                let invocation = Invocation::default();
                tokio::time::timeout(
                    Duration::from_secs(2),
                    invocation.before_spawn(&exclusive, "benchmark", &CancellationToken::new()),
                )
                .await??;
                ensure!(
                    !process_alive(ids[1])?,
                    "shell root still alive at exclusive admission"
                );
                let descendant_alive = process_alive(ids[0])?;
                // The legacy PTY backend closes ConPTY on root exit. Preserve that
                // existing console behaviour; inherited *pipe* survival is asserted above.
                ensure!(
                    descendant_alive || console_teardown,
                    "descendant did not survive root completion"
                );
                if descendant_alive {
                    pipe.write_all(b"x").await?;
                }
                let mut end = Vec::new();
                pipe.read_to_end(&mut end).await?;
                drop(invocation);
            }
            support::respond(socket, index, commands.get(index).cloned()).await?;
        }
        Ok::<_, anyhow::Error>(requests)
    });
    let (_, requests) = tokio::time::timeout(Duration::from_secs(60), async {
        tokio::try_join!(run_codex(&home, &cwd, &bin, &rules), async {
            fixture.await?
        })
    })
    .await??;
    ensure!(!support::outputs(&requests).contains("No scheduling ticket"));
    if console_teardown {
        println!(
            "PASS patched Codex {label}: root exit releases gate independently of existing ConPTY teardown"
        );
    } else {
        println!(
            "PASS patched Codex {label}: exclusive admitted with root gone and inherited-output descendant alive"
        );
    }
    Ok(())
}

async fn retry_case(repo: &Path) -> Result<()> {
    let base = repo.join(format!(".local/scheduler-retry-{}", std::process::id()));
    let cwd = base.join("workspace");
    let outside = base.join("outside");
    std::fs::create_dir_all(&cwd)?;
    std::fs::create_dir_all(&outside)?;
    let log = cwd.join("attempts.txt");
    let target = outside.join("result.txt");
    let home = base.join("home");
    let bin = repo.join(".local/scheduler-target/debug");
    let rules = repo.join("tools/agent-scheduler/scheduling.rules");
    let http = TcpListener::bind("127.0.0.1:0").await?;
    configure(&home, http.local_addr()?, "unelevated")?;
    let command = "codex_lifecycle.exe retry-write attempts.txt ../outside/result.txt";
    let fixture = tokio::spawn(async move {
        let commands = [
            json!({"cmd":"agent-scheduler ticket shared retry-test","login":false}),
            json!({"cmd":command,"shell":"cmd.exe","login":false,"yield_time_ms":1000}),
            json!({"cmd":"agent-scheduler ticket shared immediate-next","login":false}),
            json!({"cmd":"Write-Output SCHEDULED","login":false}),
        ];
        let mut requests = Vec::new();
        for index in 0..=commands.len() {
            let (socket, request) = support::request(&http).await?;
            requests.push(request);
            support::respond(socket, index, commands.get(index).cloned()).await?;
        }
        Ok::<_, anyhow::Error>(requests)
    });
    let approvals = tokio::time::timeout(
        Duration::from_secs(45),
        run_codex(&home, &cwd, &bin, &rules),
    )
    .await??;
    let requests = fixture.await??;
    let outputs = support::outputs(&requests);
    let attempts = std::fs::read_to_string(&log)
        .with_context(|| format!("missing attempts log; outputs: {outputs}"))?;
    ensure!(
        attempts == "denied\nsuccess\n",
        "expected genuine sandbox denial and successful retry, got {attempts:?}; outputs: {outputs}"
    );
    ensure!(std::fs::read_to_string(target)? == "retry succeeded");
    ensure!(approvals.iter().any(|item| {
        item["params"]["command"]
            .as_str()
            .is_some_and(|cmd| cmd.contains("retry-write"))
    }));
    ensure!(
        !outputs.contains("No scheduling ticket") && !outputs.contains("ticket already exists"),
        "{outputs}"
    );
    println!(
        "PASS genuine sandbox write denial and approved Codex retry under one ticket; immediate next ticket"
    );
    Ok(())
}

fn main() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();
    if helper(&args)? {
        return Ok(());
    }
    tokio::runtime::Runtime::new()?.block_on(async {
        let repo = std::env::current_dir()?;
        retry_case(&repo).await?;
        for (sandbox, tty) in [
            ("none", false),
            ("none", true),
            ("unelevated", false),
            ("unelevated", true),
        ] {
            root_case(&repo, sandbox, tty).await?;
        }
        Ok(())
    })
}
