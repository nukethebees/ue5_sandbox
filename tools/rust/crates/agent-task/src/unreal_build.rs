use clap::Parser;
use std::ffi::OsString;
use std::path::{Path, PathBuf};
use std::process::Command;

#[derive(Parser)]
#[command(
    name = "agent-task unreal-build",
    about = "Invoke Unreal's build script"
)]
struct Arguments {
    #[arg(long)]
    build_script: PathBuf,
    #[arg(long)]
    target: String,
    #[arg(long)]
    platform: String,
    #[arg(long)]
    configuration: String,
    #[arg(long)]
    project: PathBuf,
    #[arg(long)]
    native_toolchain: String,
}

fn file(path: &Path, description: &str) -> Result<PathBuf, String> {
    if path.as_os_str().is_empty() || !path.is_file() {
        return Err(format!(
            "The {description} file is missing: '{}'",
            path.display()
        ));
    }
    std::path::absolute(path).map_err(|e| format!("Invalid {description} path: {e}"))
}

pub fn run(arguments: &[OsString]) -> Result<i32, String> {
    let args = match Arguments::try_parse_from(
        std::iter::once(OsString::from("agent-task unreal-build")).chain(arguments.iter().cloned()),
    ) {
        Ok(args) => args,
        Err(error) => {
            let code = error.exit_code();
            let _ = error.print();
            return Ok(code);
        }
    };
    for value in [
        &args.target,
        &args.platform,
        &args.configuration,
        &args.native_toolchain,
    ] {
        if value.trim().is_empty() {
            return Err("Unreal build arguments must not be empty".into());
        }
    }
    let script = file(&args.build_script, "build script")?;
    let project = file(&args.project, "project")?;
    let mut project_argument = OsString::from("-Project=");
    project_argument.push(project);

    // Rust's Windows Command implementation invokes .bat/.cmd through cmd.exe
    // and quotes batch arguments, including paths containing spaces.
    let status = Command::new(&script)
        .args([&args.target, &args.platform, &args.configuration])
        .arg(project_argument)
        .arg("-WaitMutex")
        .env("IOJ_NATIVE_TOOLCHAIN", &args.native_toolchain)
        // The cooperative jobs board does not own or reap MSBuild descendants.
        .env("MSBUILDDISABLENODEREUSE", "1")
        .status()
        .map_err(|e| {
            format!(
                "Unable to start Unreal build script '{}': {e}",
                script.display()
            )
        })?;
    let code = status.code().unwrap_or(1);
    if !status.success() {
        eprintln!("Unreal build script exited with code {code}");
    }
    Ok(code)
}
