//! Drive the actual patched Codex with scripted local Responses events. No model service is used.
use anyhow::{Context, Result, ensure};
use serde_json::{Value, json};
use std::time::Duration;
mod support;
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
    let installed_bin = std::env::args_os().nth(1).map(std::path::PathBuf::from);
    let bin = installed_bin
        .clone()
        .unwrap_or_else(|| repo.join(".local/scheduler-target/debug"));
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
        let mut commands = vec![
            "rg --version",
            "Write-Output NO_TICKET",
            "agent-scheduler ticket shared smoke",
            "Write-Output FORBIDDEN",
            "Write-Output SCHEDULED",
        ];
        for _ in 0..12 {
            commands.push("agent-scheduler ticket shared immediate-next");
            commands.push("Write-Output SCHEDULED");
        }
        commands.push("agent-scheduler status");
        let mut requests = Vec::new();
        for index in 0..=commands.len() {
            let (socket, request) = support::request(&listener).await?;
            if index > 0 {
                let previous = commands[index - 1];
                if previous.starts_with("agent-scheduler ticket")
                    || previous == "Write-Output SCHEDULED"
                {
                    let id = format!("call_{}", index - 1);
                    let output = request["input"]
                        .as_array()
                        .context("missing input")?
                        .iter()
                        .find(|item| {
                            item["type"] == "function_call_output" && item["call_id"] == id
                        })
                        .and_then(|item| item["output"].as_str())
                        .context("missing command result")?;
                    ensure!(
                        output.contains("Process exited with code 0"),
                        "Immediate ticket/command failed: {output}"
                    );
                }
            }
            requests.push(request);
            support::respond(
                socket,
                index,
                commands
                    .get(index)
                    .map(|cmd| json!({"cmd":cmd,"login":false,"yield_time_ms":30000})),
            )
            .await?;
            println!("fixture response {index}");
        }
        Ok::<_, anyhow::Error>(requests)
    });
    let mut command = if installed_bin.is_some() {
        let mut launcher = Command::new("pwsh");
        launcher
            .args(["-NoProfile", "-File"])
            .arg(bin.join("agent-codex.ps1"));
        launcher
    } else {
        Command::new(bin.join("codex.exe"))
    };
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
    ensure!(
        !support::outputs(&requests).contains("ticket already exists"),
        "Immediate ticket reuse raced release acknowledgement"
    );
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
        "PASS real Codex: exemption, missing ticket, unchanged security rejection, 12 immediate ticket/command cycles, automatic release"
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
