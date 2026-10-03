mod cli;
mod compare;
mod frame_revision;
mod gpu;
mod ismc;
mod kernel;
mod native;
mod plots;
mod results;
mod revision;
mod support;
#[cfg(test)]
#[path = "../tests/unit/mod.rs"]
mod tests;
mod time;
mod tracy;
mod unreal;

use clap::Parser;
use cli::{Cli, Command};
use support::*;

fn dispatch_benchmark_command(args: &[String]) -> Result<()> {
    let cli = Cli::try_parse_from(
        std::iter::once("benchmark-tools").chain(args.iter().map(String::as_str)),
    )?;
    let root = || find_repository_root(&std::env::current_dir()?);
    match cli.command {
        Command::Plot(options) => plots::run(&options),
        Command::Compare(options) => compare::run_native_comparison(&options, &args[1..]),
        Command::TracyReport(options) => tracy::generate_tracy_report(&options),
        Command::SandboxIsmcReport(options) => ismc::regenerate_comparison_reports(&options),
        Command::NativeSimulation(options) => {
            print!("{}", native::run_simulation_benchmark(&root()?, &options)?);
            Ok(())
        }
        Command::FighterSimulation(options) => native::run_fighter_benchmark(&root()?, &options),
        Command::FrameMemoryLevel(options) => {
            native::run_frame_memory_benchmark(&root()?, &options)
        }
        Command::FrameMemoryRevisionAb(options) => {
            frame_revision::run_frame_memory_comparison(&root()?, &options, &args[1..])
        }
        Command::KernelReport(options) => kernel::generate_kernel_report(&root()?, &options),
        Command::GpuStarfield(options) => gpu::run_starfield_benchmark(&root()?, &options),
        Command::SandboxIsmc(options) => {
            ismc::run_ismc_benchmark(&root()?, &options, &args[1..], false)
        }
        Command::SandboxIsmcRevisionAb(options) => {
            ismc::run_ismc_benchmark(&root()?, &options, &args[1..], true)
        }
        Command::Spark(options) => {
            unreal::run_unreal_benchmark(&root()?, unreal::Benchmark::Spark(options))
        }
        Command::LevelTelemetry(options) => {
            unreal::run_unreal_benchmark(&root()?, unreal::Benchmark::Telemetry(options))
        }
        Command::Heatmap(options) => {
            unreal::run_unreal_benchmark(&root()?, unreal::Benchmark::Heatmap(options))
        }
        Command::Radar3d(options) => {
            unreal::run_unreal_benchmark(&root()?, unreal::Benchmark::Radar(options))
        }
        Command::Scatter3d(options) => {
            unreal::run_unreal_benchmark(&root()?, unreal::Benchmark::Scatter(options))
        }
        Command::VolumeHeatmap3d(options) => {
            unreal::run_unreal_benchmark(&root()?, unreal::Benchmark::VolumeHeatmap(options))
        }
        Command::EntityOverlay(options) => {
            unreal::run_unreal_benchmark(&root()?, unreal::Benchmark::EntityOverlay(options))
        }
    }
}
fn main() {
    let args: Vec<_> = std::env::args().skip(1).collect();
    if let Err(error) = dispatch_benchmark_command(&args) {
        if let Some(error) = error.downcast_ref::<clap::Error>() {
            error.exit();
        }
        eprintln!("benchmark-tools: {error}");
        std::process::exit(
            error
                .downcast_ref::<ProcessFailure>()
                .map_or(1, |failure| failure.0),
        );
    }
}
