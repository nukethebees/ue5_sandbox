use crate::Scheduler;
use anyhow::{Result, bail};
use std::sync::{Arc, Mutex};
use tokio_util::sync::CancellationToken;

/// One logical tool call. Keep this owner alive across the executor's approval/retry loop.
#[derive(Default)]
pub struct Invocation {
    active: Mutex<Option<Arc<Active>>>,
}

struct Active {
    scheduler: Arc<Scheduler>,
    generation: u64,
    attempt: Mutex<AttemptState>,
}

#[derive(Default)]
struct AttemptState {
    number: u64,
    disposition: Disposition,
    root_exit_code: Option<i32>,
}

#[derive(Default, PartialEq)]
enum Disposition {
    #[default]
    Undecided,
    Accepted,
    Finished,
}

/// One physical attempt's root-exit observer; it neither spawns nor owns the process.
#[derive(Clone)]
pub struct Attempt {
    active: Option<Arc<Active>>,
    number: u64,
}

impl std::fmt::Debug for Attempt {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("Attempt")
    }
}

impl Invocation {
    /// Called after normal security decisions, immediately before each physical spawn.
    /// The first attempt claims the explicit ticket; retries reuse that claim.
    pub async fn before_spawn(
        &self,
        scheduler: &Arc<Scheduler>,
        command: &str,
        cancellation: &CancellationToken,
    ) -> Result<Attempt> {
        scheduler.check_health()?;
        if scheduler.exempt(command) {
            if cancellation.is_cancelled() {
                bail!("Scheduling wait cancelled; command was not launched");
            }
            return Ok(Attempt {
                active: None,
                number: 0,
            });
        }
        let active = {
            let mut slot = self.active.lock().unwrap();
            if slot.is_none() {
                *slot = Some(Arc::new(Active {
                    scheduler: Arc::clone(scheduler),
                    generation: scheduler.claim()?,
                    attempt: Mutex::new(AttemptState::default()),
                }));
            }
            Arc::clone(slot.as_ref().unwrap())
        };
        scheduler
            .wait_granted(active.generation, cancellation)
            .await?;
        let number = {
            let mut attempt = active.attempt.lock().unwrap();
            if attempt.disposition != Disposition::Undecided {
                bail!("Logical command already accepted; cannot spawn another attempt");
            }
            attempt.number += 1;
            attempt.root_exit_code = None;
            attempt.number
        };
        scheduler.record_command(command);
        Ok(Attempt {
            active: Some(active),
            number,
        })
    }

    /// The executor accepted this attempt and will not internally retry it.
    /// If its root has already exited, release now; otherwise the observer releases it.
    pub fn accept(&self) {
        if let Some(active) = self.active.lock().unwrap().as_ref() {
            let mut attempt = active.attempt.lock().unwrap();
            attempt.disposition = Disposition::Accepted;
            if let Some(code) = attempt.root_exit_code {
                active.scheduler.release(active.generation, Some(code));
                attempt.disposition = Disposition::Finished;
            }
        }
    }
}

impl Attempt {
    /// Called by the executor's real root-exit event, before publishing completion.
    pub fn root_exited(&self, code: Option<i32>) {
        let Some(active) = &self.active else { return };
        let mut attempt = active.attempt.lock().unwrap();
        if attempt.number != self.number || attempt.disposition == Disposition::Finished {
            return;
        }
        let Some(code) = code else {
            active
                .scheduler
                .fail("Root exit notification was lost; restart this Codex session".into());
            return;
        };
        attempt.root_exit_code = Some(code);
        if attempt.disposition == Disposition::Accepted {
            active.scheduler.release(active.generation, Some(code));
            attempt.disposition = Disposition::Finished;
        }
    }
}

impl Drop for Invocation {
    fn drop(&mut self) {
        if let Some(active) = self.active.get_mut().unwrap().take() {
            let mut attempt = active.attempt.lock().unwrap();
            if attempt.disposition == Disposition::Undecided {
                // Spawn failure, denied retry, or cancellation. The session explicitly stays
                // Releasing until the daemon acknowledges; Drop never pretends release is done.
                active
                    .scheduler
                    .release(active.generation, attempt.root_exit_code);
                attempt.disposition = Disposition::Finished;
            }
        }
    }
}
