use std::ffi::OsString;
use std::path::PathBuf;
use std::process::Command;

const USAGE: &str = "agent-task jobs request shared|exclusive NAME\nagent-task jobs check|start|end|cancel ID\nagent-task jobs status\nAppend --json for JSON output. Request does not wait; check until Ready, then start immediately before work and end when it returns.";

pub fn run(arguments: &[OsString]) -> Result<i32, String> {
    if arguments.is_empty() || arguments == ["--help"] || arguments == ["-h"] {
        println!("{USAGE}");
        return Ok(0);
    }

    if !["request", "check", "start", "end", "cancel", "status"]
        .iter()
        .any(|name| arguments[0] == *name)
    {
        return Err(USAGE.into());
    }

    let local = std::env::var_os("LOCALAPPDATA").ok_or("LOCALAPPDATA is unavailable")?;
    let executable = PathBuf::from(local).join("NukeTheBees/jobserver/bin/jobserver.exe");
    let result = Command::new(&executable)
        .args(arguments)
        .status()
        .map_err(|error| {
            format!(
                "Cannot run jobs client '{}': {error}. Ask the maintainer to install jobserver.",
                executable.display()
            )
        })?;

    Ok(result.code().unwrap_or(1))
}
