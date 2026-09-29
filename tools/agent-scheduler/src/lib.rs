//! One connection, one explicit ticket, one logical command boundary.
mod transport;

use anyhow::{Context, Result, bail};
use codex_execpolicy::{Decision, Policy, PolicyParser};
use codex_shell_command::powershell::parse_powershell_script_into_plain_commands;
use serde_json::{Value, json};
use std::future::Future;
use std::sync::Arc;
use tokio::sync::{Mutex, OnceCell, OwnedMutexGuard, mpsc, watch};
use transport::{connect, daemon_endpoint, read_frame, write_frame};

static SCHEDULER: OnceCell<Result<Arc<Scheduler>, String>> = OnceCell::const_new();

#[derive(Clone, Debug, PartialEq)]
enum State {
    None,
    Queued,
    Granted,
    Failed(String),
}

struct Scheduler {
    policy: Policy,
    state: watch::Sender<State>,
    outgoing: mpsc::UnboundedSender<Value>,
    operation: Arc<Mutex<()>>,
}

/// The sole Codex entry point. Connection setup and control commands stay inside the wrapper.
pub async fn run<T>(
    command: Option<&str>,
    execution: impl Future<Output = T>,
    control_output: impl FnOnce(String) -> T,
) -> Result<T> {
    let Some(command) = command else {
        return Ok(execution.await);
    };
    let scheduler = SCHEDULER
        .get_or_init(|| async { open_canonical().await.map_err(|error| format!("{error:#}")) })
        .await
        .as_ref()
        .map_err(|error| anyhow::anyhow!("{error}"))?;
    if let Some(response) = scheduler.control(command).await? {
        return Ok(control_output(response));
    }
    scheduler.run(command, execution).await
}

async fn open_canonical() -> Result<Arc<Scheduler>> {
    let local = std::env::var_os("LOCALAPPDATA").context("LOCALAPPDATA is missing")?;
    let rules =
        std::path::PathBuf::from(local).join("NukeTheBees/config/agent-scheduler/scheduling.rules");
    let source = std::fs::read_to_string(&rules).with_context(|| {
        format!(
            "Missing scheduling rules: {}; ask the maintainer to configure them",
            rules.display()
        )
    })?;
    let mut parser = PolicyParser::new();
    parser.parse(&rules.to_string_lossy(), &source)?;
    let pipe = connect(&daemon_endpoint()?).await?;
    Scheduler::open(parser.build(), pipe).await
}

impl Scheduler {
    async fn open<S>(policy: Policy, mut pipe: S) -> Result<Arc<Self>>
    where
        S: tokio::io::AsyncRead + tokio::io::AsyncWrite + Unpin + Send + 'static,
    {
        write_frame(
            &mut pipe,
            &json!({"type":"hello","protocol":{"major":3,"minor":0}}),
        )
        .await?;
        let hello = read_frame(&mut pipe).await?;
        if hello["type"] != "hello_ack" || hello["protocol"]["major"] != 3 {
            bail!("Jobserver rejected scheduler connection: {hello}");
        }
        let (state, _) = watch::channel(State::None);
        let (outgoing, mut messages) = mpsc::unbounded_channel();
        let scheduler = Arc::new(Self {
            policy,
            state: state.clone(),
            outgoing,
            operation: Arc::new(Mutex::new(())),
        });
        // This task owns the sole connection. EOF permanently fails this client.
        tokio::spawn(async move {
            let (mut reader, mut writer) = tokio::io::split(pipe);
            let reading = async {
                loop {
                    let message = read_frame(&mut reader).await?;
                    let next = match message["type"].as_str() {
                        Some("queued") => State::Queued,
                        Some("granted") => State::Granted,
                        Some("released") => State::None,
                        _ => bail!("Jobserver protocol error: {message}"),
                    };
                    state.send_replace(next);
                }
                #[allow(unreachable_code)]
                Ok::<(), anyhow::Error>(())
            };
            let writing = async {
                while let Some(message) = messages.recv().await {
                    write_frame(&mut writer, &message).await?;
                }
                bail!("Scheduler closed")
            };
            let result: Result<()> = tokio::select! {
                result = reading => result,
                result = writing => result,
            };
            state.send_replace(State::Failed(format!(
                "Jobserver connection lost: {}",
                result.unwrap_err()
            )));
        });
        Ok(scheduler)
    }

    fn healthy_state(&self) -> Result<State> {
        let state = self.state.borrow().clone();
        if let State::Failed(error) = state {
            bail!("{error}");
        }
        Ok(state)
    }

    fn lock(&self) -> Result<OwnedMutexGuard<()>> {
        self.operation.clone().try_lock_owned()
            .context("A scheduler action or logical command is already in progress; nested scheduling is unsupported")
    }

    async fn changed(&self, receiver: &mut watch::Receiver<State>) -> Result<State> {
        receiver
            .changed()
            .await
            .context("Jobserver connection closed")?;
        self.healthy_state()
    }

    async fn ticket(&self, mode: &str, name: &str) -> Result<Value> {
        let _operation = self.lock()?;
        if !matches!(mode, "shared" | "exclusive") || name.is_empty() || name.len() > 1024 {
            bail!("Use ticket shared|exclusive NAME (1 to 1024 bytes)");
        }
        self.healthy_state()?;
        if !self.state.send_if_modified(|state| {
            if *state != State::None {
                return false;
            }
            *state = State::Queued;
            true
        }) {
            self.healthy_state()?;
            bail!("A ticket already exists; finish the command or clear it");
        }
        let mut changed = self.state.subscribe();
        self.outgoing
            .send(json!({"type":"request","mode":mode,"name":name}))?;
        self.changed(&mut changed).await?;
        self.status()
    }

    fn status(&self) -> Result<Value> {
        Ok(json!({"state":match self.healthy_state()? {
            State::None => "none", State::Queued => "queued", State::Granted => "granted",
            State::Failed(_) => unreachable!(),
        }}))
    }

    async fn release(&self) -> Result<()> {
        let mut changed = self.state.subscribe();
        if self.healthy_state()? == State::None {
            return Ok(());
        }
        self.outgoing.send(json!({"type":"release"}))?;
        loop {
            if self.changed(&mut changed).await? == State::None {
                return Ok(());
            }
        }
    }

    async fn clear(&self) -> Result<Value> {
        let _operation = self.lock()?;
        self.release().await?;
        self.status()
    }

    fn exempt(&self, command: &str) -> bool {
        parse_powershell_script_into_plain_commands(command).is_some_and(|commands| {
            !commands.is_empty()
                && commands.iter().all(|argv| {
                    self.policy.check(argv, &|_| Decision::Prompt).decision == Decision::Allow
                })
        })
    }

    /// The future is the existing logical Codex operation, including all sandbox retries.
    async fn run<T>(
        self: &Arc<Self>,
        command: &str,
        execution: impl Future<Output = T>,
    ) -> Result<T> {
        self.healthy_state()?;
        if self.exempt(command) {
            return Ok(execution.await);
        }
        let operation = self.lock()?;
        if self.healthy_state()? == State::None {
            bail!(
                "No scheduling ticket. Run agent-scheduler ticket shared NAME (exclusive for benchmarks) as a separate action"
            );
        }
        let mut completion = Completion {
            scheduler: self.clone(),
            operation: Some(operation),
        };
        let mut changed = self.state.subscribe();
        loop {
            if self.healthy_state()? == State::Granted {
                break;
            }
            self.changed(&mut changed).await?;
        }
        let result = execution.await;
        self.release().await?;
        completion.operation.take();
        Ok(result)
    }

    /// Recognize only a standalone pseudo-command; nothing is spawned.
    async fn control(&self, command: &str) -> Result<Option<String>> {
        let Some(commands) = parse_powershell_script_into_plain_commands(command) else {
            return Ok(None);
        };
        if commands.len() != 1 || commands[0].first().map(String::as_str) != Some("agent-scheduler")
        {
            return Ok(None);
        }
        let argv = &commands[0];
        let response = match argv.get(1).map(String::as_str) {
            Some("ticket") if argv.len() == 4 => self.ticket(&argv[2], &argv[3]).await?,
            Some("status") if argv.len() == 2 => self.status()?,
            Some("clear") if argv.len() == 2 => self.clear().await?,
            _ => bail!(
                "Use agent-scheduler ticket shared|exclusive NAME, status, or clear as a separate command"
            ),
        };
        Ok(Some(response.to_string()))
    }
}

// Dropping a cancelled logical call releases its ticket too. No process observation.
struct Completion {
    scheduler: Arc<Scheduler>,
    operation: Option<OwnedMutexGuard<()>>,
}

impl Drop for Completion {
    fn drop(&mut self) {
        if let Some(operation) = self.operation.take() {
            let scheduler = self.scheduler.clone();
            tokio::spawn(async move {
                let _operation = operation;
                let _ = scheduler.release().await;
            });
        }
    }
}

#[cfg(test)]
mod tests;
