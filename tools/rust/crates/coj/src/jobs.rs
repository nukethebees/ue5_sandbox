use clap::{Parser, Subcommand, ValueEnum};
use jobserver_client::Client;
use serde_json::{Value, json};
use std::ffi::OsString;
use std::fmt::Write;
use std::path::{Path, PathBuf};

#[derive(Parser)]
#[command(
    name = "coj jobs",
    about = "Coordinate heavyweight work on the cooperative jobs board.",
    after_help = "Request a ticket, check until Ready, start immediately before work, and end after the command returns. No command starts, monitors, or kills processes."
)]
struct Cli {
    /// Print the successful server response as JSON.
    #[arg(long, global = true)]
    json: bool,
    #[command(subcommand)]
    operation: Operation,
}

#[derive(Clone, Copy, ValueEnum)]
enum Mode {
    Shared,
    Exclusive,
}

#[derive(Subcommand)]
enum Operation {
    /// Request a shared or exclusive ticket; return immediately.
    Request {
        mode: Mode,
        #[arg(value_parser = nonempty)]
        name: String,
        /// Override the inherited Codex name (otherwise use the worktree directory name).
        #[arg(long, value_parser = nonempty)]
        owner: Option<String>,
    },
    /// Read an active ticket.
    Check {
        #[arg(value_parser = clap::value_parser!(u64).range(1..))]
        id: u64,
    },
    /// Mark a Ready ticket Running, immediately before work.
    Start {
        #[arg(value_parser = clap::value_parser!(u64).range(1..))]
        id: u64,
    },
    /// Remove a Running ticket after work returns; never stop processes.
    End {
        #[arg(value_parser = clap::value_parser!(u64).range(1..))]
        id: u64,
    },
    /// Remove an unused Queued or Ready ticket.
    Cancel {
        #[arg(value_parser = clap::value_parser!(u64).range(1..))]
        id: u64,
    },
    /// Manually remove tickets in any state; never stop processes.
    Clear {
        #[arg(required_unless_present = "owner", conflicts_with = "owner", value_parser = clap::value_parser!(u64).range(1..))]
        id: Option<u64>,
        /// Clear this owner's tickets across worktrees unless --worktree is specified.
        #[arg(long, value_parser = nonempty)]
        owner: Option<String>,
        /// Limit owner clearing to this worktree path.
        #[arg(long, requires = "owner")]
        worktree: Option<PathBuf>,
    },
    /// List active tickets with their owner and worktree.
    Status,
    /// Check that the daemon is reachable.
    Ping,
    /// Stop the daemon only when the board is empty.
    Shutdown,
}

fn nonempty(value: &str) -> Result<String, String> {
    if value.trim().is_empty() {
        Err("A nonempty name is required.".into())
    } else {
        Ok(value.into())
    }
}

fn normalized_path(path: &Path) -> Result<String, String> {
    let path = path
        .canonicalize()
        .map_err(|e| format!("Resolve '{}': {e}", path.display()))?;
    let text = path.to_str().ok_or("Worktree path is not valid UTF-8")?;
    #[cfg(windows)]
    {
        Ok(text.strip_prefix(r"\\?\").unwrap_or(text).to_lowercase())
    }
    #[cfg(not(windows))]
    {
        Ok(text.into())
    }
}

fn ticket_owner(explicit: Option<String>) -> Result<(String, String), String> {
    let cwd = std::env::current_dir().map_err(|e| e.to_string())?;
    let root = cwd
        .ancestors()
        .find(|path| path.join(".git").exists())
        .unwrap_or(&cwd);
    let worktree = normalized_path(root)?;
    let inherited_root = std::env::var_os("COJ_CODEX_WORKTREE")
        .map(|root| normalized_path(Path::new(&root)))
        .transpose()?;
    let inherited_name = if inherited_root.as_deref() == Some(&worktree) {
        std::env::var("COJ_CODEX_NAME").ok()
    } else {
        None
    };
    let owner = explicit.or(inherited_name).unwrap_or_else(|| {
        root.file_name()
            .unwrap_or(root.as_os_str())
            .to_string_lossy()
            .into_owned()
    });
    Ok((nonempty(&owner)?, worktree))
}

pub fn run(arguments: &[OsString]) -> Result<i32, String> {
    let cli = match Cli::try_parse_from(
        std::iter::once(OsString::from("coj jobs")).chain(arguments.iter().cloned()),
    ) {
        Ok(cli) => cli,
        Err(error) => {
            let code = error.exit_code();
            error.print().map_err(|e| e.to_string())?;
            return Ok(code);
        }
    };
    let message = match cli.operation {
        Operation::Request { mode, name, owner } => {
            let (owner, worktree) = ticket_owner(owner)?;
            json!({"type":"request", "mode":match mode { Mode::Shared => "shared", Mode::Exclusive => "exclusive" }, "name":name, "owner":owner, "worktree":worktree})
        }
        Operation::Check { id } => json!({"type":"check", "id":id}),
        Operation::Start { id } => json!({"type":"start", "id":id}),
        Operation::End { id } => json!({"type":"end", "id":id}),
        Operation::Cancel { id } => json!({"type":"cancel", "id":id}),
        Operation::Clear { id: Some(id), .. } => json!({"type":"clear", "id":id}),
        Operation::Clear {
            owner, worktree, ..
        } => {
            let mut message = json!({"type":"clear", "owner":owner});
            if let Some(path) = worktree {
                message["worktree"] = normalized_path(&path)?.into();
            }
            message
        }
        Operation::Status => json!({"type":"status"}),
        Operation::Ping => json!({"type":"ping"}),
        Operation::Shutdown => json!({"type":"shutdown"}),
    };
    let reply = Client::for_current_user()?.request(message)?;
    if cli.json {
        println!("{reply}");
    } else {
        print!("{}", format_reply(&reply)?);
    }
    Ok(0)
}

fn field<'a>(value: &'a Value, key: &str) -> Result<&'a str, String> {
    value[key]
        .as_str()
        .ok_or_else(|| format!("Jobs-board response is missing '{key}'."))
}

pub(crate) fn ticket_line(ticket: &Value) -> Result<String, String> {
    let id = ticket["id"]
        .as_u64()
        .ok_or("Jobs-board response is missing ticket ID.")?;
    Ok(format!(
        "{id:<6} {:<10} {:<10} {:<16} {}  [{}]\n",
        field(ticket, "mode")?,
        field(ticket, "state")?,
        field(ticket, "owner")?,
        field(ticket, "name")?,
        field(ticket, "worktree")?
    ))
}

fn format_reply(reply: &Value) -> Result<String, String> {
    match field(reply, "type")? {
        "status" => {
            let tickets = reply["tickets"]
                .as_array()
                .ok_or("Jobs-board response is missing tickets.")?;
            if tickets.is_empty() {
                return Ok("Board is empty.\n".into());
            }
            let mut output = format!(
                "{:<6} {:<10} {:<10} {:<16} {}\n",
                "ID", "Mode", "State", "Owner", "Description  [Worktree]"
            );
            for state in ["Running", "Ready", "Queued"] {
                for ticket in tickets.iter().filter(|ticket| ticket["state"] == state) {
                    output.push_str(&ticket_line(ticket)?);
                }
            }
            Ok(output)
        }
        "ticket" => ticket_line(reply),
        "cleared" => {
            let tickets = reply["tickets"]
                .as_array()
                .ok_or("Jobs-board response is missing cleared tickets.")?;
            let mut output = String::new();
            for ticket in tickets {
                output.push_str(&ticket_line(ticket)?);
            }
            writeln!(
                output,
                "Cleared {} tickets from the board. Processes were not affected.",
                tickets.len()
            )
            .unwrap();
            Ok(output)
        }
        kind => Ok(format!("{kind}\n")),
    }
}
