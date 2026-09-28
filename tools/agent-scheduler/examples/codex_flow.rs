//! Drive the actual patched Codex with scripted local Responses events. No model service is used.
use anyhow::{Context, Result, ensure};
use serde_json::{Value, json};
use std::time::Duration;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpListener;
use tokio::process::Command;

fn strings(value: &Value, result: &mut Vec<String>) {
    match value {
        Value::String(text) => result.push(text.clone()),
        Value::Array(items) => items.iter().for_each(|item| strings(item, result)),
        Value::Object(items) => items.values().for_each(|item| strings(item, result)),
        _ => {}
    }
}

#[tokio::main]
async fn main() -> Result<()> {
    let repo = std::env::current_dir()?;
    let bin = repo.join(".local/scheduler-target/debug");
    let home = repo.join(".local/codex-flow-example");
    std::fs::create_dir_all(home.join("rules"))?;
    std::fs::write(
        home.join("rules/default.rules"),
        concat!(
            "prefix_rule(pattern=[\"Write-Output\", \"FORBIDDEN\"], decision=\"forbidden\")\n",
            "prefix_rule(pattern=[\"Write-Output\", \"SCHEDULED\"], decision=\"allow\")\n",
            "prefix_rule(pattern=[\"Write-Output\", \"NO_TICKET\"], decision=\"allow\")\n",
            "prefix_rule(pattern=[\"rg\", \"--version\"], decision=\"allow\")\n",
            "prefix_rule(pattern=[\"agent-scheduler\", [\"ticket\", \"status\", \"clear\"]], decision=\"allow\")\n"
        ),
    )?;
    let listener = TcpListener::bind("127.0.0.1:0").await?;
    let address = listener.local_addr()?;
    std::fs::write(
        home.join("config.toml"),
        format!(
            r#"
model = "gpt-5.4"
model_provider = "fixture"
approval_policy = "never"
sandbox_mode = "workspace-write"
[model_providers.fixture]
name = "local scheduler fixture"
base_url = "http://{address}/v1"
wire_api = "responses"
requires_openai_auth = false
"#
        ),
    )?;
    let server = tokio::spawn(async move {
        let commands = [
            "rg --version",
            "Write-Output NO_TICKET",
            "agent-scheduler ticket shared smoke",
            "Write-Output FORBIDDEN",
            "Write-Output SCHEDULED",
            "agent-scheduler status",
        ];
        let mut requests = Vec::new();
        for index in 0..=commands.len() {
            let (mut socket, _) = listener.accept().await?;
            let mut header = Vec::new();
            while !header.ends_with(b"\r\n\r\n") {
                header.push(socket.read_u8().await?);
                ensure!(header.len() < 65536, "HTTP header too large");
            }
            let header = String::from_utf8(header)?;
            ensure!(
                header.starts_with("POST /v1/responses "),
                "Unexpected request: {header}"
            );
            let size: usize = header
                .lines()
                .find_map(|line| {
                    let (name, value) = line.split_once(':')?;
                    name.eq_ignore_ascii_case("content-length")
                        .then(|| value.trim().parse())
                        .transpose()
                        .ok()
                        .flatten()
                })
                .context("Missing Content-Length")?;
            ensure!(size < 16 * 1024 * 1024, "Fixture request too large");
            let mut body = vec![0; size];
            socket.read_exact(&mut body).await?;
            requests.push(serde_json::from_slice::<Value>(&body)?);
            let item = if index < commands.len() {
                json!({"type":"function_call","call_id":format!("call_{index}"),"name":"exec_command","arguments":json!({"cmd":commands[index],"login":false,"yield_time_ms":1000}).to_string()})
            } else {
                json!({"type":"message","id":"done","role":"assistant","content":[{"type":"output_text","text":"SCHEDULER_FLOW_COMPLETE"}]})
            };
            let events = [
                json!({"type":"response.created","response":{"id":format!("response_{index}")}}),
                json!({"type":"response.output_item.done","item":item}),
                json!({"type":"response.completed","response":{"id":format!("response_{index}"),"usage":{"input_tokens":0,"output_tokens":0,"total_tokens":0}}}),
            ];
            let response = events
                .iter()
                .map(|event| format!("data: {event}\n\n"))
                .collect::<String>();
            socket.write_all(format!("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{response}", response.len()).as_bytes()).await?;
            println!("fixture response {index}");
        }
        Ok::<_, anyhow::Error>(requests)
    });
    let mut command = Command::new(bin.join("codex.exe"));
    command
        .args([
            "exec",
            "--json",
            "--skip-git-repo-check",
            "Run the scripted scheduler example.",
        ])
        .env("CODEX_HOME", &home)
        .env(
            "AGENT_SCHEDULER_RULES",
            repo.join("tools/agent-scheduler/scheduling.rules"),
        )
        .env(
            "PATH",
            format!("{};{}", bin.display(), std::env::var("PATH")?),
        )
        .kill_on_drop(true);
    let output = tokio::time::timeout(Duration::from_secs(120), command.output()).await??;
    println!("{}", String::from_utf8_lossy(&output.stdout));
    eprintln!("{}", String::from_utf8_lossy(&output.stderr));
    ensure!(output.status.success(), "Patched Codex failed");
    let requests = tokio::time::timeout(Duration::from_secs(2), server).await???;
    let mut text = Vec::new();
    for request in &requests {
        strings(&request["input"], &mut text);
    }
    ensure!(
        text.iter()
            .any(|item| item.contains("No scheduling ticket")),
        "Missing-ticket failure was not returned to Codex"
    );
    ensure!(
        text.iter().any(|item| item.contains("forbidden")
            || item.contains("Forbidden")
            || item.contains("policy forbids")),
        "Normal approval policy did not reject the forbidden command"
    );
    ensure!(
        text.iter()
            .any(|item| item.contains("SCHEDULED") && item.contains("Process exited with code 0")),
        "Scheduled command did not complete"
    );
    ensure!(
        text.iter().any(|item| item.contains("\"ticket\":false")),
        "Ticket was not released"
    );
    println!(
        "PASS real Codex: exempt command, missing ticket, unchanged security rejection, scheduled execution, automatic release"
    );
    command.env(
        "NUKETHEBEES_JOBSERVER_TEST_PIPE",
        format!(r"\\.\pipe\MissingSchedulerExample.{}", std::process::id()),
    );
    let unavailable = tokio::time::timeout(Duration::from_secs(10), command.output()).await??;
    ensure!(
        !unavailable.status.success(),
        "Codex started without a jobserver"
    );
    ensure!(
        String::from_utf8_lossy(&unavailable.stderr).contains("Cannot connect"),
        "Missing startup health diagnostic: {}",
        String::from_utf8_lossy(&unavailable.stderr)
    );
    println!("PASS Codex startup fails when the jobserver is unavailable");
    Ok(())
}
