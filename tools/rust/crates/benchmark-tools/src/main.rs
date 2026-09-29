mod frame_revision;
mod gpu;
mod ismc;
mod native;
mod results;
mod revision;
mod support;
#[cfg(test)]
mod tests;

use support::*;

const USAGE: &str = "Usage: benchmark-tools <native-simulation|fighter-simulation|frame-memory-level|frame-memory-revision-ab|level-telemetry|gpu-starfield|sandbox-ismc|sandbox-ismc-revision-ab|sandbox-ismc-report> [options]\nSee docs/benchmarks.md for workload options.";

fn execute(args: &[String]) -> Result<()> {
    if args.iter().any(|arg| arg == "--help" || arg == "-h") {
        println!("{USAGE}");
        return Ok(());
    }
    let (command, args) = args
        .split_first()
        .ok_or("A benchmark command is required.")?;
    if command == "sandbox-ismc-report" {
        return ismc::report(args);
    }
    let root = repository(&std::env::current_dir()?)?;
    match command.as_str() {
        "native-simulation" => {
            print!("{}", native::simulation(&root, args)?);
            Ok(())
        }
        "fighter-simulation" => native::fighter(&root, args),
        "frame-memory-level" => native::frame(&root, args),
        "frame-memory-revision-ab" => frame_revision::execute(&root, args),
        "level-telemetry" => native::telemetry(&root, args),
        "gpu-starfield" => gpu::execute(&root, args),
        "sandbox-ismc" => ismc::execute(&root, args, false),
        "sandbox-ismc-revision-ab" => ismc::execute(&root, args, true),
        _ => Err(format!("Unknown benchmark command '{command}'.\n{USAGE}").into()),
    }
}
fn main() {
    let args: Vec<_> = std::env::args().skip(1).collect();
    if let Err(error) = execute(&args) {
        eprintln!("benchmark-tools: {error}");
        std::process::exit(
            error
                .downcast_ref::<ProcessFailure>()
                .map_or(1, |failure| failure.0),
        );
    }
}
