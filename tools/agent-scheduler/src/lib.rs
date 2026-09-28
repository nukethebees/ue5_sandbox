//! Windows-only example: scheduling policy and lease lifetime, without process execution.
mod transport;

use anyhow::{Context, Result, bail};
use codex_execpolicy::{Decision, Policy, PolicyParser};
use codex_shell_command::powershell::parse_powershell_script_into_plain_commands;
use serde_json::{Value, json};
use std::sync::{Arc, Mutex};
use tokio::net::windows::named_pipe::ServerOptions;
use tokio::sync::{OnceCell, mpsc, oneshot, watch};
use tokio_util::sync::CancellationToken;
use transport::{WATCHDOG, connect, daemon_endpoint, read_frame, write_frame};

static SCHEDULER: OnceCell<Arc<Scheduler>> = OnceCell::const_new();

#[derive(Default)]
struct State {
    generation: u64,
    ticket: bool,
    claimed: bool,
    lease: Option<u64>,
    granted: bool,
    finishing: bool,
    running: bool,
    error: Option<String>,
}

pub struct Scheduler {
    client_id: u64,
    endpoint: String,
    policy: Policy,
    state: Mutex<State>,
    changed: watch::Sender<u64>,
    outgoing: mpsc::UnboundedSender<Value>,
    lost: CancellationToken,
}

/// Establish one live jobserver session for this Codex process before accepting work.
pub async fn initialize() -> Result<&'static Arc<Scheduler>> {
    SCHEDULER
        .get_or_try_init(|| async {
            let rules = std::env::var("AGENT_SCHEDULER_RULES")
                .context("Set AGENT_SCHEDULER_RULES to the scheduling exemption rules file")?;
            Scheduler::open(&rules).await
        })
        .await
}

impl Scheduler {
    pub async fn open(rules: &str) -> Result<Arc<Self>> {
        let mut parser = PolicyParser::new();
        parser.parse(rules, &std::fs::read_to_string(rules)?)?;
        Self::connect(parser.build()).await
    }

    async fn connect(policy: Policy) -> Result<Arc<Self>> {
        let mut pipe = connect(&daemon_endpoint()?).await?;
        write_frame(&mut pipe, &json!({"type":"hello", "protocol":{"major":2,"minor":0}, "client_version":"codex-example"})).await?;
        let hello = tokio::time::timeout(WATCHDOG, read_frame(&mut pipe)).await??;
        if hello["type"] != "hello_ack" || hello["protocol"]["major"] != 2 {
            bail!("Jobserver handshake failed: {hello}");
        }
        let client_id = hello["client"].as_u64().context("Missing ClientId")?;
        let endpoint = format!(r"\\.\pipe\NukeTheBees.CodexScheduler.{client_id}");
        let listener = ServerOptions::new()
            .first_pipe_instance(true)
            .create(&endpoint)?;
        let (outgoing, mut messages) = mpsc::unbounded_channel();
        let (changed, _) = watch::channel(0);
        let scheduler = Arc::new(Self {
            client_id,
            endpoint,
            policy,
            state: Mutex::new(State::default()),
            changed,
            outgoing,
            lost: CancellationToken::new(),
        });
        let (mut reader, mut writer) = tokio::io::split(pipe);
        let weak = Arc::downgrade(&scheduler);
        let lost = scheduler.lost.clone();
        tokio::spawn(async move {
            loop {
                let result = tokio::select! {
                    _ = lost.cancelled() => break,
                    result = tokio::time::timeout(WATCHDOG, read_frame(&mut reader)) => result,
                };
                let Some(scheduler) = weak.upgrade() else {
                    break;
                };
                match result {
                    Ok(Ok(message)) => scheduler.received(message),
                    Ok(Err(error)) => {
                        scheduler.fail(format!("Jobserver disconnected: {error}"));
                        break;
                    }
                    Err(_) => {
                        scheduler.fail("Jobserver unresponsive: no traffic for 20 seconds".into());
                        break;
                    }
                }
            }
        });
        let weak = Arc::downgrade(&scheduler);
        let lost = scheduler.lost.clone();
        tokio::spawn(async move {
            loop {
                let message = tokio::select! {
                    _ = lost.cancelled() => break,
                    message = messages.recv() => message,
                };
                let Some(message) = message else { break };
                if let Err(error) = write_frame(&mut writer, &message).await {
                    if let Some(scheduler) = weak.upgrade() {
                        scheduler.fail(error.to_string());
                    }
                    break;
                }
            }
        });
        let weak = Arc::downgrade(&scheduler);
        let endpoint = scheduler.endpoint.clone();
        tokio::spawn(async move {
            let mut listener = listener;
            loop {
                if listener.connect().await.is_err() {
                    break;
                }
                let mut connection = listener;
                listener = match ServerOptions::new().create(&endpoint) {
                    Ok(listener) => listener,
                    Err(_) => break,
                };
                let weak = weak.clone();
                tokio::spawn(async move {
                    if let Ok(Ok(request)) =
                        tokio::time::timeout(WATCHDOG, read_frame(&mut connection)).await
                    {
                        if let Some(scheduler) = weak.upgrade() {
                            let response = scheduler.control(request).await;
                            let _ = write_frame(&mut connection, &response).await;
                        }
                    }
                });
            }
        });
        eprintln!("scheduler connected: client={client_id}");
        Ok(scheduler)
    }

    fn fail(&self, error: String) {
        self.state.lock().unwrap().error = Some(error);
        self.lost.cancel();
        self.changed.send_modify(|revision| *revision += 1);
    }

    fn received(&self, message: Value) {
        let mut state = self.state.lock().unwrap();
        match message["type"].as_str() {
            Some("heartbeat" | "started") => return,
            Some("queued" | "granted") => {
                state.lease = message["lease"].as_u64();
                state.granted = message["type"] == "granted";
            }
            Some("released" | "cancelled") => {
                *state = State {
                    generation: state.generation,
                    ..State::default()
                }
            }
            _ => {
                state.error = Some(format!("Jobserver protocol error: {message}"));
                self.lost.cancel();
            }
        }
        self.changed.send_modify(|revision| *revision += 1);
    }

    async fn control(&self, request: Value) -> Value {
        let mut changed = self.changed.subscribe();
        let response = self.control_now(request);
        if response["state"] != "requested" {
            return response;
        }
        loop {
            {
                let state = self.state.lock().unwrap();
                if let Some(error) = &state.error {
                    return json!({"error":error});
                }
                if let Some(lease) = state.lease {
                    return json!({"state":if state.granted {"granted"} else {"queued"},"client":self.client_id,"lease":lease});
                }
                if !state.ticket {
                    return json!({"error":"Ticket was cleared"});
                }
            }
            if changed.changed().await.is_err() {
                return json!({"error":"Scheduler stopped"});
            }
        }
    }

    fn control_now(&self, request: Value) -> Value {
        let mut state = self.state.lock().unwrap();
        if let Some(error) = &state.error {
            return json!({"error":error});
        }
        match request["type"].as_str() {
            Some("ticket") => {
                let mode = request["mode"].as_str().unwrap_or("");
                if !matches!(mode, "shared" | "exclusive") {
                    return json!({"error":"Choose shared or exclusive"});
                }
                if state.ticket {
                    return json!({"error":"A ticket already exists; finish the command or clear it"});
                }
                state.ticket = true;
                state.generation += 1;
                let metadata = json!({"name":request["name"].as_str().unwrap_or("Codex command"), "client":"codex-example"});
                let _ = self.outgoing.send(json!({"type":"acquire", "gates":[{"name":"machine","mode":mode}], "metadata":metadata}));
                json!({"state":"requested", "client":self.client_id})
            }
            Some("clear") => {
                if state.running || state.claimed && state.granted {
                    return json!({"error":"Cancel the running command through Codex"});
                }
                if state.ticket && !state.finishing {
                    state.finishing = true;
                    let _ = self.outgoing.send(json!({"type":"cancel"}));
                    self.changed.send_modify(|revision| *revision += 1);
                }
                json!({"state":"clearing"})
            }
            Some("status") => {
                json!({"client":self.client_id,"ticket":state.ticket,"granted":state.granted,"claimed":state.claimed,"lease":state.lease})
            }
            _ => {
                json!({"error":"Use agent-scheduler ticket shared|exclusive NAME, status, or clear"})
            }
        }
    }

    /// Check exactly the displayed PowerShell command without executing or rewriting it.
    pub async fn before_spawn(
        self: &Arc<Self>,
        command: &str,
        cancellation: &CancellationToken,
    ) -> Result<Option<Permit>> {
        if self.lost.is_cancelled() {
            bail!("Jobserver connection lost; restart this Codex session");
        }
        let exempt = parse_powershell_script_into_plain_commands(command).is_some_and(|commands| {
            !commands.is_empty()
                && commands.iter().all(|argv| {
                    self.policy.check(argv, &|_| Decision::Prompt).decision == Decision::Allow
                })
        });
        if exempt {
            return Ok(None);
        }
        let mut changed = self.changed.subscribe();
        let generation = {
            let mut state = self.state.lock().unwrap();
            if !state.ticket || state.finishing {
                bail!(
                    "No scheduling ticket. Run agent-scheduler ticket shared NAME (or exclusive for a benchmark), then retry the command."
                );
            }
            if state.claimed {
                bail!("This broker already has a command; wait for it to finish");
            }
            state.claimed = true;
            state.generation
        };
        let permit = Permit {
            scheduler: Arc::clone(self),
            command: command.into(),
            generation,
            started: false,
            finished: false,
        };
        loop {
            {
                let state = self.state.lock().unwrap();
                if let Some(error) = &state.error {
                    bail!("{error}");
                }
                if state.generation != generation
                    || !state.ticket
                    || state.finishing
                    || cancellation.is_cancelled()
                {
                    bail!("Scheduling wait cancelled; command was not launched");
                }
                if state.granted {
                    return Ok(Some(permit));
                }
            }
            tokio::select! {
                _ = cancellation.cancelled() => bail!("Scheduling wait cancelled; command was not launched"),
                result = changed.changed() => { result?; }
            }
        }
    }

    pub fn endpoint(&self) -> &str {
        &self.endpoint
    }
    pub fn lost(&self) -> CancellationToken {
        self.lost.clone()
    }
}

pub struct Permit {
    scheduler: Arc<Scheduler>,
    command: String,
    generation: u64,
    started: bool,
    finished: bool,
}

impl Permit {
    pub fn started(&mut self) {
        self.started = true;
        self.scheduler.state.lock().unwrap().running = true;
        let _ = self
            .scheduler
            .outgoing
            .send(json!({"type":"health","state":format!("local command: {}", self.command)}));
    }

    pub fn finish(mut self, exit_code: i32) {
        self.release(exit_code);
    }

    fn release(&mut self, exit_code: i32) {
        if self.finished {
            return;
        }
        self.finished = true;
        let mut state = self.scheduler.state.lock().unwrap();
        if state.generation != self.generation || !state.ticket || state.finishing {
            return;
        }
        state.finishing = true;
        let message = if self.started {
            json!({"type":"release","lease":state.lease,"exit_code":exit_code})
        } else {
            json!({"type":"cancel"})
        };
        let _ = self.scheduler.outgoing.send(message);
    }

    /// Preserve Codex's existing exit receiver while releasing at root exit, without output draining.
    pub fn on_root_exit(mut self, exit: oneshot::Receiver<i32>) -> oneshot::Receiver<i32> {
        self.started();
        let (sender, receiver) = oneshot::channel();
        tokio::spawn(async move {
            match exit.await {
                Ok(code) => {
                    self.finish(code);
                    let _ = sender.send(code);
                }
                Err(_) => {
                    self.scheduler
                        .fail("Root exit notification was lost".into());
                }
            }
        });
        receiver
    }
}

impl Drop for Permit {
    fn drop(&mut self) {
        self.release(-1);
    }
}

impl Drop for Scheduler {
    fn drop(&mut self) {
        self.lost.cancel();
    }
}

pub async fn control(request: Value) -> Result<Value> {
    let endpoint = std::env::var("AGENT_SCHEDULER_SESSION")
        .context("Run this command inside the patched Codex session")?;
    let mut pipe = connect(&endpoint).await?;
    write_frame(&mut pipe, &request).await?;
    let response = tokio::time::timeout(WATCHDOG, read_frame(&mut pipe)).await??;
    if let Some(error) = response.get("error") {
        bail!("{error}");
    }
    Ok(response)
}
