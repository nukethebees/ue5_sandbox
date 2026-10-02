use clap::{Parser, Subcommand};
use std::ffi::OsString;

#[cfg(windows)]
mod windows;

#[derive(Parser)]
#[command(
    name = "agent-task codex",
    about = "Own a local Codex session and its Windows process job",
    after_help = "Each command requires a session name:\n  agent-task codex start <NAME>\n  agent-task codex processes <NAME>\n  agent-task codex clean <NAME> [--dry-run]\n\nExample: agent-task codex start dev1"
)]
struct Cli {
    #[command(subcommand)]
    operation: Operation,
}

#[derive(Subcommand)]
enum Operation {
    /// Launch Codex in this terminal with --no-daemon inside a Windows job.
    Start {
        #[arg(value_parser = session_name)]
        name: String,
    },
    /// List verified members of a named session in this worktree.
    Processes {
        #[arg(value_parser = session_name)]
        name: String,
    },
    /// Stop child work, preserving Codex runtime processes and this command's ancestors.
    Clean {
        #[arg(value_parser = session_name)]
        name: String,
        #[arg(long)]
        dry_run: bool,
    },
}

fn session_name(value: &str) -> Result<String, String> {
    if value.is_empty()
        || value.len() > 64
        || !value
            .bytes()
            .all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || matches!(c, b'-' | b'_'))
    {
        return Err("Use 1–64 lowercase letters, digits, '-' or '_' for the session name.".into());
    }
    Ok(value.into())
}

pub fn run(arguments: &[OsString]) -> Result<i32, String> {
    let cli = match Cli::try_parse_from(
        std::iter::once(OsString::from("agent-task codex")).chain(arguments.iter().cloned()),
    ) {
        Ok(cli) => cli,
        Err(error) => {
            let code = error.exit_code();
            error.print().map_err(|e| e.to_string())?;
            return Ok(code);
        }
    };

    #[cfg(windows)]
    {
        windows::execute_codex_command(cli.operation)
    }
    #[cfg(not(windows))]
    {
        let _ = cli;
        Err("Codex process jobs require Windows.".into())
    }
}
