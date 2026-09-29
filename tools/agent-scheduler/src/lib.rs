//! Windows-only example: scheduling policy and lease lifetime, without process execution.
mod invocation;
mod transport;
pub use invocation::{Attempt, Invocation};

use anyhow::{Context, Result, bail};
use codex_execpolicy::{Decision, Policy, PolicyParser};
use codex_shell_command::powershell::parse_powershell_script_into_plain_commands;
use serde_json::{Value, json};
use std::sync::{Arc, Mutex};
use tokio::sync::{OnceCell, mpsc, watch};
use tokio_util::sync::CancellationToken;
use transport::{WATCHDOG, connect, daemon_endpoint, read_frame, session_listener, write_frame};

static SCHEDULER: OnceCell<Arc<Scheduler>> = OnceCell::const_new();

#[derive(Default)]
struct State {
    generation: u64,
    phase: Phase,
}

#[derive(Default)]
enum Phase {
    #[default]
    Idle,
    Ticket {
        admission: Admission,
        owner: Owner,
    },
    Failed(String),
}

#[derive(Clone, Copy)]
enum Admission {
    Requested,
    Queued(u64),
    Granted(u64),
}

impl Admission {
    fn lease(self) -> Option<u64> {
        match self {
            Self::Requested => None,
            Self::Queued(id) | Self::Granted(id) => Some(id),
        }
    }
}

#[derive(Clone, Copy, PartialEq)]
enum Owner {
    Available,
    Invocation,
    Releasing,
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
        Self::connect(parser.build(), &daemon_endpoint()?).await
    }

    async fn connect(policy: Policy, daemon: &str) -> Result<Arc<Self>> {
        let mut pipe = connect(daemon)
            .await
            .context("Start the jobserver before Codex")?;
        write_frame(&mut pipe, &json!({"type":"hello", "protocol":{"major":2,"minor":0}, "client_version":"codex-example"})).await?;
        let hello = tokio::time::timeout(WATCHDOG, read_frame(&mut pipe)).await??;
        if hello["type"] != "hello_ack" || hello["protocol"]["major"] != 2 {
            bail!("Jobserver handshake failed: {hello}");
        }
        let client_id = hello["client"].as_u64().context("Missing ClientId")?;
        let endpoint = format!(r"\\.\pipe\NukeTheBees.CodexScheduler.{client_id}");
        let listener = session_listener(&endpoint, true)?;
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
        let lost = scheduler.lost.clone();
        tokio::spawn(async move {
            let mut listener = listener;
            loop {
                let connected = tokio::select! {
                    _ = lost.cancelled() => break,
                    result = listener.connect() => result,
                };
                if connected.is_err() {
                    break;
                }
                let mut connection = listener;
                listener = match session_listener(&endpoint, false) {
                    Ok(listener) => listener,
                    Err(_) => break,
                };
                let weak = weak.clone();
                tokio::spawn(async move {
                    if let Ok(Ok(request)) =
                        tokio::time::timeout(WATCHDOG, read_frame(&mut connection)).await
                    {
                        if let Some(scheduler) = weak.upgrade() {
                            let cancellation = CancellationToken::new();
                            let response = {
                                use tokio::io::AsyncReadExt;
                                let response = scheduler.control(request, &cancellation);
                                tokio::pin!(response);
                                tokio::select! {
                                    response = &mut response => response,
                                    _ = connection.read_u8() => {
                                        cancellation.cancel();
                                        response.await
                                    }
                                }
                            };
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
        self.state.lock().unwrap().phase = Phase::Failed(error);
        self.lost.cancel();
        self.changed.send_modify(|revision| *revision += 1);
    }

    fn received(&self, message: Value) {
        let mut state = self.state.lock().unwrap();
        if matches!(state.phase, Phase::Failed(_)) {
            return;
        }
        let kind = message["type"].as_str().unwrap_or("");
        if matches!(kind, "heartbeat" | "started") {
            return;
        }
        let valid = match (&mut state.phase, kind, message["lease"].as_u64()) {
            (Phase::Ticket { admission, .. }, "queued" | "granted", Some(id))
                if admission.lease().is_none_or(|previous| previous == id) =>
            {
                *admission = if kind == "queued" {
                    Admission::Queued(id)
                } else {
                    Admission::Granted(id)
                };
                true
            }
            (
                Phase::Ticket {
                    admission,
                    owner: Owner::Releasing,
                },
                "released" | "cancelled",
                Some(id),
            ) if admission.lease().is_none_or(|previous| previous == id) => {
                state.phase = Phase::Idle;
                true
            }
            _ => false,
        };
        if !valid {
            state.phase = Phase::Failed(format!("Jobserver protocol error: {message}"));
            self.lost.cancel();
        }
        self.changed.send_modify(|revision| *revision += 1);
    }

    async fn control(&self, request: Value, cancellation: &CancellationToken) -> Value {
        let result = match request["type"].as_str() {
            Some("ticket") => {
                self.request_ticket(
                    request["mode"].as_str().unwrap_or(""),
                    request["name"].as_str().unwrap_or("Codex command"),
                    cancellation,
                )
                .await
            }
            Some("status") => self.status(),
            Some("clear") => self.clear(),
            _ => Err(anyhow::anyhow!(
                "Use agent-scheduler ticket shared|exclusive NAME, status, or clear"
            )),
        };
        result.unwrap_or_else(|error| json!({"error":error.to_string()}))
    }

    /// Wait out the preceding release acknowledgement before reserving a new generation.
    pub async fn request_ticket(
        &self,
        mode: &str,
        name: &str,
        cancellation: &CancellationToken,
    ) -> Result<Value> {
        if !matches!(mode, "shared" | "exclusive") {
            bail!("Choose shared or exclusive");
        }
        let mut changed = self.changed.subscribe();
        let generation = loop {
            if cancellation.is_cancelled() {
                bail!("Ticket request cancelled");
            }
            {
                let mut state = self.state.lock().unwrap();
                match &state.phase {
                    Phase::Failed(error) => bail!("{error}"),
                    Phase::Idle => {
                        state.generation += 1;
                        state.phase = Phase::Ticket {
                            admission: Admission::Requested,
                            owner: Owner::Available,
                        };
                        self.outgoing.send(
                            json!({"type":"acquire", "gates":[{"name":"machine","mode":mode}],
                            "metadata":{"name":name,"client":"codex-example"}}),
                        )?;
                        break state.generation;
                    }
                    Phase::Ticket {
                        owner: Owner::Releasing,
                        ..
                    } => {}
                    Phase::Ticket { .. } => {
                        bail!("A ticket already exists; finish the command or clear it")
                    }
                }
            }
            tokio::select! {
                _ = cancellation.cancelled() => bail!("Ticket request cancelled"),
                result = changed.changed() => { result?; }
            }
        };
        loop {
            if cancellation.is_cancelled() {
                self.release(generation, None);
                bail!("Ticket request cancelled");
            }
            {
                let state = self.state.lock().unwrap();
                if let Phase::Failed(error) = &state.phase {
                    bail!("{error}");
                }
                if state.generation != generation {
                    bail!("Ticket was cleared");
                }
                match &state.phase {
                    Phase::Ticket {
                        owner: Owner::Releasing,
                        ..
                    }
                    | Phase::Idle => bail!("Ticket was cleared"),
                    Phase::Ticket { admission, .. } => {
                        if let Some(lease) = admission.lease() {
                            return Ok(
                                json!({"state":if matches!(admission, Admission::Granted(_)) {"granted"} else {"queued"},"client":self.client_id,"lease":lease}),
                            );
                        }
                    }
                    Phase::Failed(_) => unreachable!(),
                }
            }
            tokio::select! {
                _ = cancellation.cancelled() => {},
                result = changed.changed() => { result?; }
            }
        }
    }

    pub fn status(&self) -> Result<Value> {
        let state = self.state.lock().unwrap();
        match &state.phase {
            Phase::Failed(error) => bail!("{error}"),
            Phase::Idle => Ok(json!({"client":self.client_id,"state":"idle","ticket":false})),
            Phase::Ticket { admission, owner } => Ok(json!({
                "client":self.client_id,"ticket":true,"lease":admission.lease(),
                "claimed":*owner == Owner::Invocation,
                "state":match owner {
                    Owner::Releasing => "releasing",
                    Owner::Invocation if matches!(admission, Admission::Granted(_)) => "running",
                    _ => match admission { Admission::Requested => "requested", Admission::Queued(_) => "queued", Admission::Granted(_) => "granted" }
                }
            })),
        }
    }

    pub fn clear(&self) -> Result<Value> {
        let mut state = self.state.lock().unwrap();
        match &mut state.phase {
            Phase::Failed(error) => bail!("{error}"),
            Phase::Ticket {
                admission: Admission::Granted(_),
                owner: Owner::Invocation,
            } => bail!("Cancel the running command through Codex"),
            Phase::Ticket { owner, .. } if *owner != Owner::Releasing => {
                *owner = Owner::Releasing;
                let _ = self.outgoing.send(json!({"type":"cancel"}));
                self.changed.send_modify(|revision| *revision += 1);
            }
            _ => {}
        }
        Ok(json!({"state":"clearing"}))
    }

    fn check_health(&self) -> Result<()> {
        if let Phase::Failed(error) = &self.state.lock().unwrap().phase {
            bail!("{error}");
        }
        Ok(())
    }

    fn exempt(&self, command: &str) -> bool {
        parse_powershell_script_into_plain_commands(command).is_some_and(|commands| {
            !commands.is_empty()
                && commands.iter().all(|argv| {
                    self.policy.check(argv, &|_| Decision::Prompt).decision == Decision::Allow
                })
        })
    }

    fn claim(&self) -> Result<u64> {
        let mut state = self.state.lock().unwrap();
        match &mut state.phase {
            Phase::Failed(error) => bail!("{error}"),
            Phase::Ticket {
                owner: Owner::Invocation,
                ..
            } => bail!("This session already has a logical command; wait for it to finish"),
            Phase::Ticket { owner, .. } if *owner == Owner::Available => {
                *owner = Owner::Invocation;
                Ok(state.generation)
            }
            _ => bail!(
                "No scheduling ticket. Run agent-scheduler ticket shared NAME (or exclusive for a benchmark), then retry the command."
            ),
        }
    }

    async fn wait_granted(&self, generation: u64, cancellation: &CancellationToken) -> Result<()> {
        let mut changed = self.changed.subscribe();
        loop {
            {
                let state = self.state.lock().unwrap();
                if let Phase::Failed(error) = &state.phase {
                    bail!("{error}");
                }
                if state.generation != generation || cancellation.is_cancelled() {
                    bail!("Scheduling wait cancelled; command was not launched");
                }
                match state.phase {
                    Phase::Ticket {
                        owner: Owner::Invocation,
                        admission: Admission::Granted(_),
                    } => return Ok(()),
                    Phase::Ticket {
                        owner: Owner::Invocation,
                        ..
                    } => {}
                    _ => bail!("Scheduling wait cancelled; command was not launched"),
                }
            }
            tokio::select! {
                _ = cancellation.cancelled() => bail!("Scheduling wait cancelled; command was not launched"),
                result = changed.changed() => { result?; }
            }
        }
    }

    fn record_command(&self, command: &str) {
        let _ = self
            .outgoing
            .send(json!({"type":"health","state":format!("local command: {command}")}));
    }

    fn release(&self, generation: u64, exit_code: Option<i32>) {
        let mut state = self.state.lock().unwrap();
        if state.generation != generation {
            return;
        }
        if let Phase::Ticket { admission, owner } = &mut state.phase {
            if *owner == Owner::Releasing {
                return;
            }
            *owner = Owner::Releasing;
            let message = match (admission, exit_code) {
                (Admission::Granted(lease), Some(code)) => {
                    json!({"type":"release","lease":lease,"exit_code":code})
                }
                _ => json!({"type":"cancel"}),
            };
            let _ = self.outgoing.send(message);
            self.changed.send_modify(|revision| *revision += 1);
        }
    }

    pub fn endpoint(&self) -> &str {
        &self.endpoint
    }
    pub fn lost(&self) -> CancellationToken {
        self.lost.clone()
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
    let mut pipe = connect(&endpoint)
        .await
        .context("Cannot reach this Codex session's scheduler control pipe")?;
    write_frame(&mut pipe, &request).await?;
    let response = tokio::time::timeout(WATCHDOG, read_frame(&mut pipe)).await??;
    if let Some(error) = response.get("error") {
        bail!("{error}");
    }
    Ok(response)
}

#[cfg(test)]
mod tests;
