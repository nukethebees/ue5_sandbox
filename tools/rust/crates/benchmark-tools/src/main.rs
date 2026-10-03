mod frame_revision;
mod gpu;
mod ismc;
mod kernel;
mod native;
mod results;
mod revision;
mod support;
#[cfg(test)]
#[path = "../tests/unit/mod.rs"]
mod tests;
mod tracy;
mod unreal;

use support::*;

const USAGE: &str = "Usage: benchmark-tools <kernel-report|spark|heatmap|radar-3d|scatter-3d|volume-heatmap-3d|entity-overlay|native-simulation|fighter-simulation|frame-memory-level|frame-memory-revision-ab|level-telemetry|gpu-starfield|sandbox-ismc|sandbox-ismc-revision-ab|sandbox-ismc-report> [options]\nSee docs/benchmarks.md for workload options.";

fn dispatch_benchmark_command(args: &[String]) -> Result<()> {
    if args.first().is_some_and(|arg| arg == "tracy-report") {
        return tracy::generate_tracy_report(&args[1..]);
    }
    if args.iter().any(|arg| arg == "--help" || arg == "-h") {
        println!(
            "{USAGE}\n  tracy-report --trace <capture.tracy> [options]  Offline zone statistics"
        );
        return Ok(());
    }
    let (command, args) = args
        .split_first()
        .ok_or("A benchmark command is required.")?;
    if command == "sandbox-ismc-report" {
        return ismc::regenerate_comparison_reports(args);
    }
    let root = find_repository_root(&std::env::current_dir()?)?;
    match command.as_str() {
        "native-simulation" => {
            print!("{}", native::run_simulation_benchmark(&root, args)?);
            Ok(())
        }
        "fighter-simulation" => native::run_fighter_benchmark(&root, args),
        "frame-memory-level" => native::run_frame_memory_benchmark(&root, args),
        "frame-memory-revision-ab" => frame_revision::run_frame_memory_comparison(&root, args),
        "kernel-report" => kernel::generate_kernel_report(&root, args),
        "spark" | "level-telemetry" | "heatmap" | "radar-3d" | "scatter-3d"
        | "volume-heatmap-3d" | "entity-overlay" => {
            unreal::run_unreal_benchmark(&root, command, args)
        }
        "gpu-starfield" => gpu::run_starfield_benchmark(&root, args),
        "sandbox-ismc" => ismc::run_ismc_benchmark(&root, args, false),
        "sandbox-ismc-revision-ab" => ismc::run_ismc_benchmark(&root, args, true),
        _ => Err(format!("Unknown benchmark command '{command}'.\n{USAGE}").into()),
    }
}
fn main() {
    let args: Vec<_> = std::env::args().skip(1).collect();
    if let Err(error) = dispatch_benchmark_command(&args) {
        eprintln!("benchmark-tools: {error}");
        std::process::exit(
            error
                .downcast_ref::<ProcessFailure>()
                .map_or(1, |failure| failure.0),
        );
    }
}
