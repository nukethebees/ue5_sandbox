use clap::{Args, Parser, Subcommand};
use std::ffi::OsString;

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

    println!("[1/3] Preparing submodules");
    crate::update_submodules(&root)?;

    println!("[2/3] Configuring native build (run prepare-worktree first)");
    crate::run(&root, "cmake", &["--preset", "native"])?;

    println!("[3/3] Installing canonical jobserver");
    let target = if force {
        "install-jobserver-force"
    } else {
        "install-jobserver"
    };
    crate::run(
        &root,
        "cmake",
        &["--build", "--preset", "native", "--target", target],
    )
}
