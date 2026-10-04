use clap::{Args, Parser, Subcommand};
use std::ffi::OsString;
use std::fs;
use std::path::Path;

#[cfg(windows)]
mod windows;

#[derive(Parser)]
#[command(
    name = "coj install",
    about = "Install/update canonical per-user build tools."
)]
struct Cli {
    #[command(subcommand)]
    tool: Tool,
}

#[derive(Subcommand)]
enum Tool {
    /// Install all central tools (currently jobserver).
    CentralTools(InstallOptions),
    /// Install and register the per-user jobs board.
    Jobserver(InstallOptions),
}

#[derive(Args)]
struct InstallOptions {
    /// Close and report active jobserver tickets before replacement; leave their processes running.
    #[arg(long)]
    force: bool,
}

pub fn run(arguments: &[OsString]) -> Result<i32, String> {
    let cli = match Cli::try_parse_from(
        std::iter::once(OsString::from("coj install")).chain(arguments.iter().cloned()),
    ) {
        Ok(cli) => cli,
        Err(error) => {
            let code = error.exit_code();
            error.print().map_err(|e| e.to_string())?;
            return Ok(code);
        }
    };

    match cli.tool {
        Tool::CentralTools(options) | Tool::Jobserver(options) => install_jobserver(options.force)?,
    }
    Ok(0)
}

fn install_jobserver(force: bool) -> Result<(), String> {
    let root = crate::worktree_root()?;

    println!("[1/4] Preparing submodules");
    crate::update_submodules(&root)?;

    println!("[2/4] Configuring native build (run prepare-worktree first)");
    crate::run(&root, "cmake", &["--preset", "native"])?;

    let manifest_path = root.join("out/build/native/jobserver-install.json");
    let manifest: serde_json::Value = serde_json::from_slice(
        &fs::read(&manifest_path).map_err(|e| format!("Read {}: {e}", manifest_path.display()))?,
    )
    .map_err(|e| format!("Invalid jobserver build metadata: {e}"))?;
    let asan_enabled = manifest["asan_enabled"]
        .as_bool()
        .ok_or("Jobserver build metadata is missing asan_enabled.")?;
    if asan_enabled {
        return Err("An ASAN jobserver build is for local validation and must not replace the installed machine jobserver. Use a build configured with IOJ_ENABLE_ASAN=OFF for canonical installation.".into());
    }
    let daemon = Path::new(
        manifest["daemon"]
            .as_str()
            .ok_or("Jobserver build metadata is missing daemon.")?,
    );

    println!("[3/4] Building jobserverd");
    crate::run(
        &root,
        "cmake",
        &["--build", "--preset", "native", "--target", "jobserverd"],
    )?;
    if !daemon.is_absolute() || !daemon.is_file() {
        return Err(format!(
            "CMake did not produce the jobserver executable at '{}'.",
            daemon.display()
        ));
    }

    println!("[4/4] Installing canonical jobserver");
    #[cfg(windows)]
    {
        windows::install(&root, daemon, force)
    }
    #[cfg(not(windows))]
    {
        let _ = force;
        Err("Jobserver installation requires Windows.".into())
    }
}
