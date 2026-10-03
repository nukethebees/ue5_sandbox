use clap::Parser;
use std::ffi::OsString;
use std::path::Path;

#[derive(Parser)]
#[command(
    name = "coj presets",
    about = "Generate or check revision-local CMake presets"
)]
struct Arguments {
    #[arg(long)]
    check: bool,
}

pub fn generate(root: &Path, check: bool) -> Result<(), String> {
    let mut arguments = vec!["cmake/presets/generate.py"];
    if check {
        arguments.push("--check");
    }
    crate::run(root, "python", &arguments)
}

pub fn run(arguments: &[OsString]) -> Result<i32, String> {
    let args = match Arguments::try_parse_from(
        std::iter::once(OsString::from("coj presets")).chain(arguments.iter().cloned()),
    ) {
        Ok(args) => args,
        Err(error) => {
            let code = error.exit_code();
            let _ = error.print();
            return Ok(code);
        }
    };
    generate(&crate::worktree_root()?, args.check)?;
    Ok(0)
}
