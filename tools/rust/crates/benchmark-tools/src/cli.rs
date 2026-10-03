use clap::{Args, Parser, Subcommand, ValueEnum};
use std::path::PathBuf;

#[derive(Parser)]
#[command(
    name = "benchmark-tools",
    about = "Run benchmarks, compare revisions, and regenerate reports"
)]
pub struct Cli {
    #[command(subcommand)]
    pub command: Command,
}

#[derive(Subcommand)]
pub enum Command {
    /// Run kernel measurements and generate SVG charts.
    KernelReport(KernelOptions),
    /// Run an authored native simulation level.
    NativeSimulation(SimulationOptions),
    /// Measure stable fighter populations and generate charts.
    FighterSimulation(FighterOptions),
    /// Measure frame memory for the batch scenario.
    FrameMemoryLevel(FrameMemoryOptions),
    /// Compare frame memory across revisions.
    FrameMemoryRevisionAb(FrameComparisonOptions),
    /// Compare native simulation timings across revisions.
    #[command(
        after_help = "Results: raw JSON per process, captures.json, comparison.json/.csv/.md, SVG plots, source.diff.\nUse a shared jobs ticket for preparation and an exclusive ticket for measurement."
    )]
    Compare(CompareOptions),
    /// Summarize zones in a saved Tracy capture.
    #[command(
        after_help = "Requires matching tracy-csvexport on PATH. Time windows select by invocation start without clipping duration.\nPercentiles use exact nearest-rank values; exceeding event/zone limits fails rather than sampling."
    )]
    TracyReport(TracyOptions),
    /// Generate SVG plots from saved JSON results.
    Plot(PlotOptions),
    /// Measure GPU starfield rendering.
    GpuStarfield(GpuOptions),
    /// Measure SandboxISMC rendering.
    SandboxIsmc(IsmcOptions),
    /// Compare SandboxISMC across revisions.
    SandboxIsmcRevisionAb(IsmcOptions),
    /// Regenerate a saved SandboxISMC comparison.
    SandboxIsmcReport(IsmcReportOptions),
    /// Measure spark rendering through Unreal automation.
    Spark(SparkOptions),
    /// Measure level telemetry storage through Unreal automation.
    LevelTelemetry(TelemetryOptions),
    Heatmap(HeatmapOptions),
    #[command(name = "radar-3d")]
    Radar3d(RadarOptions),
    #[command(name = "scatter-3d")]
    Scatter3d(ScatterOptions),
    #[command(name = "volume-heatmap-3d")]
    VolumeHeatmap3d(EditorOptions),
    EntityOverlay(EditorOptions),
}

pub fn finite(min: f64, max: f64) -> impl Fn(&str) -> Result<f64, String> + Clone {
    move |text| {
        text.parse::<f64>()
            .ok()
            .filter(|v| v.is_finite() && (min..=max).contains(v))
            .ok_or_else(|| format!("expected a finite number from {min} to {max}"))
    }
}

fn switch(text: &str) -> Result<bool, String> {
    match text {
        "0" => Ok(false),
        "1" => Ok(true),
        _ => Err("expected 0 or 1".into()),
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, ValueEnum)]
pub enum PlotKind {
    Kernel,
    Fighter,
    Comparison,
}

#[derive(Args)]
pub struct PlotOptions {
    #[arg(long, value_enum)]
    pub kind: PlotKind,
    #[arg(long)]
    pub input: PathBuf,
    #[arg(long)]
    pub output_dir: PathBuf,
    /// Kernel backend (default: scalar, falling back to autovec-avx2).
    #[arg(long)]
    pub baseline: Option<String>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, ValueEnum)]
pub enum KernelWorkload {
    Representative,
    VectorLayout,
    Full,
    Highway,
}

#[derive(Args)]
pub struct KernelOptions {
    #[arg(long, value_enum, default_value = "representative")]
    pub workload: KernelWorkload,
    /// Defaults to 7 (3 for Highway).
    #[arg(long, value_parser = clap::value_parser!(u32).range(1..=1000))]
    pub repetitions: Option<u32>,
    /// Seconds per case; defaults to 0.05 (0.02 for Highway).
    #[arg(long, value_parser = finite(0.001, 180.0))]
    pub min_time: Option<f64>,
    #[arg(long)]
    pub skip_build: bool,
}

#[derive(Clone, Debug, Parser)]
pub struct SimulationOptions {
    #[arg(long)]
    pub level: PathBuf,
    #[arg(long, value_parser = finite(f64::MIN_POSITIVE, f64::MAX))]
    pub seconds: f64,
    #[arg(long, default_value_t = 1, value_parser = clap::value_parser!(u32).range(1..))]
    pub game_speed: u32,
    #[arg(long, conflicts_with = "fighter_stress_caps", value_parser = clap::value_parser!(u32).range(1..))]
    pub fighter_stress_cap: Option<u32>,
    #[arg(long)]
    pub fighter_stress_caps: Option<String>,
    #[arg(long, default_value_t = 5.0, value_parser = finite(0.0, 86400.0))]
    pub warmup_seconds: f64,
    #[arg(long, default_value_t = 60.0, value_parser = finite(0.1, 86400.0))]
    pub saturation_timeout_seconds: f64,
    #[arg(long, default_value = "native-simulation-benchmark")]
    pub build_preset: String,
    #[arg(long)]
    pub telemetry: bool,
    #[arg(long)]
    pub skip_build: bool,
}

#[derive(Args)]
pub struct FighterOptions {
    #[arg(long, default_value = "2000,4000")]
    pub fighter_caps: String,
    #[arg(long, default_value_t = 10.0, value_parser = finite(0.1, 86400.0))]
    pub seconds: f64,
    #[arg(long, default_value_t = 5.0, value_parser = finite(0.0, 86400.0))]
    pub warmup_seconds: f64,
    #[arg(long, default_value_t = 60.0, value_parser = finite(0.1, 86400.0))]
    pub saturation_timeout_seconds: f64,
    #[arg(long)]
    pub output_dir: Option<PathBuf>,
    #[arg(long)]
    pub skip_build: bool,
}

#[derive(Args)]
pub struct FrameMemoryOptions {
    #[arg(long, default_value_t = 20.0, value_parser = finite(0.1, 86400.0))]
    pub seconds: f64,
    #[arg(long)]
    pub skip_build: bool,
}

#[derive(Args)]
pub struct FrameComparisonOptions {
    #[arg(long, default_value_t = 5, value_parser = clap::value_parser!(u32).range(1..=100))]
    pub iterations: u32,
    #[arg(long, default_value_t = 0, value_parser = clap::value_parser!(u32).range(0..=10))]
    pub warmup_iterations: u32,
    #[arg(long, default_value = "HEAD")]
    pub baseline: String,
    #[arg(long, default_value = ".local/benchmarks/frame-memory-revision-ab")]
    pub output_dir: PathBuf,
    #[arg(long)]
    pub baseline_worktree: Option<String>,
    #[arg(long, requires = "baseline_worktree")]
    pub skip_build: bool,
    #[arg(long, conflicts_with = "validate_only")]
    pub prepare_only: bool,
    #[arg(long)]
    pub validate_only: bool,
    #[arg(long)]
    pub keep_baseline_worktree: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, ValueEnum)]
pub enum NativeWorkload {
    FighterSimulation,
    NativeSimulation,
}

#[derive(Args)]
pub struct CompareOptions {
    #[arg(long)]
    pub baseline: String,
    /// Compare a second commit; otherwise use the current checkout, including edits.
    #[arg(long)]
    pub candidate: Option<String>,
    #[arg(long, value_enum, default_value = "fighter-simulation")]
    pub workload: NativeWorkload,
    /// Required for native-simulation; relative to each revision's root.
    #[arg(long)]
    pub level: Option<String>,
    /// Fighter populations (default: 2000,4000).
    #[arg(long)]
    pub fighter_caps: Option<String>,
    /// Simulated duration per case (default: fighter 10, native 5 seconds).
    #[arg(long, value_parser = finite(0.001, 180.0))]
    pub seconds: Option<f64>,
    /// Fighter steady-state warmup (default: 5 seconds).
    #[arg(long, value_parser = finite(0.0, 180.0))]
    pub warmup_seconds: Option<f64>,
    /// Fighter saturation limit (default: 60 seconds).
    #[arg(long, value_parser = finite(0.1, 180.0))]
    pub saturation_timeout_seconds: Option<f64>,
    /// Native simulation speed (default: 1).
    #[arg(long, value_parser = clap::value_parser!(u32).range(1..))]
    pub game_speed: Option<u32>,
    /// Complete pairs alternating AB/BA (default: 2).
    #[arg(long, conflicts_with = "order", value_parser = clap::value_parser!(u32).range(1..=100))]
    pub repetitions: Option<u32>,
    /// Explicit balanced A/B order, such as ABAB or AABB; replaces repetitions.
    #[arg(long)]
    pub order: Option<String>,
    #[arg(long)]
    pub baseline_worktree: Option<String>,
    #[arg(long, requires = "candidate")]
    pub candidate_worktree: Option<String>,
    #[arg(long, default_value = ".local/benchmarks/compare")]
    pub output_dir: PathBuf,
    #[arg(long, conflicts_with = "skip_build")]
    pub prepare_only: bool,
    #[arg(long, requires = "baseline_worktree")]
    pub skip_build: bool,
    #[arg(long)]
    pub keep_worktrees: bool,
}

#[derive(Clone, Copy, Debug, ValueEnum, serde::Serialize)]
#[serde(rename_all = "lowercase")]
pub enum TracySort {
    Total,
    Max,
    Mean,
    P95,
}

#[derive(Args)]
pub struct TracyOptions {
    #[arg(long)]
    pub trace: PathBuf,
    #[arg(long, default_value = "")]
    pub filter: String,
    #[arg(long, default_value_t = 0.0, value_parser = finite(0.0, 1e9))]
    pub from_seconds: f64,
    #[arg(long, value_parser = finite(0.0, 1e9))]
    pub to_seconds: Option<f64>,
    #[arg(long)]
    pub thread: Option<u64>,
    #[arg(long, value_enum, default_value = "total")]
    pub sort: TracySort,
    #[arg(long, default_value_t = 10, value_parser = clap::value_parser!(u32).range(1..=100))]
    pub top: u32,
    #[arg(long, default_value_t = 3, value_parser = clap::value_parser!(u32).range(0..=10))]
    pub worst: u32,
    #[arg(long, default_value_t = 5_000_000, value_parser = clap::value_parser!(u32).range(1..=100_000_000))]
    pub max_events: u32,
    #[arg(long)]
    pub output: Option<PathBuf>,
    #[arg(long = "self")]
    pub self_time: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, ValueEnum)]
pub enum CameraMode {
    Stationary,
    Moving,
}

#[derive(Parser)]
pub struct GpuOptions {
    #[arg(long)]
    pub editor: Option<PathBuf>,
    #[arg(long, default_value = "Sandbox.uproject")]
    pub project: PathBuf,
    #[arg(long, default_value = "Saved/Benchmarks/GpuStarfield")]
    pub output: PathBuf,
    #[arg(long, default_value = "10000,100000,1000000")]
    pub counts: String,
    #[arg(long, default_value = "1280x720")]
    pub resolutions: String,
    #[arg(long, default_value = "1")]
    pub size_multipliers: String,
    #[arg(
        long,
        value_enum,
        value_delimiter = ',',
        ignore_case = true,
        default_value = "stationary,moving"
    )]
    pub camera_modes: Vec<CameraMode>,
    #[arg(long, default_value_t = 60, value_parser = clap::value_parser!(u32).range(1..=i32::MAX as i64))]
    pub warmup_frames: u32,
    #[arg(long, default_value_t = 180, value_parser = clap::value_parser!(u32).range(1..=i32::MAX as i64))]
    pub capture_frames: u32,
    #[arg(long, default_value_t = 3, value_parser = clap::value_parser!(u32).range(1..=i32::MAX as i64))]
    pub repeats: u32,
    #[arg(long, default_value_t = 10, value_parser = clap::value_parser!(u32).range(1..=i32::MAX as i64))]
    pub trim_frames: u32,
    #[arg(long)]
    pub build_dir: Option<String>,
    #[arg(long)]
    pub skip_build: bool,
    #[arg(long, default_value_t = 2400, value_parser = clap::value_parser!(u32).range(1..=86400))]
    pub timeout_seconds: u32,
}

#[derive(Clone, Copy, Debug, ValueEnum)]
pub enum IsmcMode {
    Paired,
    Custom,
    #[value(name = "engine_ismc")]
    EngineIsmc,
}
impl IsmcMode {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Paired => "paired",
            Self::Custom => "custom",
            Self::EngineIsmc => "engine_ismc",
        }
    }
}
#[derive(Clone, Copy, Debug, ValueEnum)]
pub enum Visibility {
    All,
    Half,
    None,
}
impl Visibility {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::All => "all",
            Self::Half => "half",
            Self::None => "none",
        }
    }
}
#[derive(Clone, Copy, Debug, ValueEnum)]
pub enum Bounds {
    Calculated,
    Supplied,
}
impl Bounds {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Calculated => "calculated",
            Self::Supplied => "supplied",
        }
    }
}
#[derive(Clone, Copy, Debug, ValueEnum)]
pub enum CustomData {
    None,
    Static,
    Animated,
}
impl CustomData {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::None => "none",
            Self::Static => "static",
            Self::Animated => "animated",
        }
    }
}

#[derive(Parser)]
pub struct IsmcOptions {
    #[arg(long)]
    pub editor: Option<PathBuf>,
    #[arg(long, default_value_t = 1280, value_parser = clap::value_parser!(u32).range(1..=16384))]
    pub width: u32,
    #[arg(long, default_value_t = 720, value_parser = clap::value_parser!(u32).range(1..=16384))]
    pub height: u32,
    #[arg(long, default_value_t = 40000, value_parser = clap::value_parser!(u32).range(1..=i32::MAX as i64))]
    pub instances: u32,
    #[arg(long, default_value_t = 100.0, value_parser = finite(0.0, 100.0))]
    pub update_percent: f64,
    #[arg(long, value_enum, default_value = "paired")]
    pub mode: IsmcMode,
    #[arg(long, value_enum, default_value = "all")]
    pub visibility: Visibility,
    #[arg(long, value_enum, default_value = "calculated")]
    pub bounds: Bounds,
    #[arg(long, value_enum, default_value = "none")]
    pub custom_data: CustomData,
    #[arg(long, default_value = "0", action = clap::ArgAction::Set, value_parser = switch)]
    pub shadows: bool,
    #[arg(long, default_value = "0", action = clap::ArgAction::Set, value_parser = switch)]
    pub churn: bool,
    #[arg(long)]
    pub min_instances: Option<u32>,
    #[arg(long, default_value_t = 120, value_parser = clap::value_parser!(u32).range(1..=i32::MAX as i64))]
    pub half_cycle_updates: u32,
    #[arg(long, default_value_t = 5.0, value_parser = finite(0.0, 100.0))]
    pub replacement_percent: f64,
    #[arg(long, default_value_t = 0, value_parser = clap::value_parser!(u32).range(0..=i32::MAX as i64))]
    pub warmup_updates: u32,
    #[arg(long, default_value_t = 1.0, value_parser = finite(0.0, 3600.0))]
    pub warmup_seconds: f64,
    #[arg(long, default_value_t = 5.0, value_parser = finite(0.01, 3600.0))]
    pub seconds: f64,
    #[arg(long, default_value = "1", action = clap::ArgAction::Set, value_parser = switch)]
    pub trace: bool,
    #[arg(long, default_value_t = 600, value_parser = clap::value_parser!(u32).range(1..=86400))]
    pub timeout_seconds: u32,
    #[arg(long)]
    pub output_dir: Option<PathBuf>,
    #[arg(long)]
    pub baseline: Option<String>,
    #[arg(long)]
    pub baseline_worktree: Option<String>,
    #[arg(long, value_parser = clap::value_parser!(u32).range(1..=100))]
    pub repetitions: Option<u32>,
    #[arg(long, default_value_t = 0, value_parser = clap::value_parser!(u32).range(0..=10))]
    pub warmup_runs: u32,
    #[arg(long, default_value = "")]
    pub label: String,
    #[arg(long)]
    pub skip_build: bool,
    #[arg(long)]
    pub keep_baseline_worktree: bool,
    #[arg(long, conflicts_with = "validate_only")]
    pub prepare_only: bool,
    #[arg(long)]
    pub validate_only: bool,
}

#[derive(Args)]
pub struct IsmcReportOptions {
    #[arg(long)]
    pub run_dir: PathBuf,
}

#[derive(Args)]
pub struct EditorOptions {
    #[arg(long)]
    pub build_dir: Option<String>,
    #[arg(long, default_value_t = 1200, value_parser = clap::value_parser!(u32).range(1..=86400))]
    pub timeout_seconds: u32,
    #[arg(long)]
    pub skip_build: bool,
}

#[derive(Parser)]
pub struct SparkOptions {
    #[command(flatten)]
    pub editor: EditorOptions,
    #[arg(long, default_value_t = 10.0, value_parser = finite(0.01, 86400.0))]
    pub seconds: f64,
    #[arg(long, default_value_t = 50000, value_parser = clap::value_parser!(u32).range(1..=i32::MAX as i64))]
    pub capacity: u32,
    #[arg(long, default_value_t = 96, value_parser = clap::value_parser!(u32).range(1..=i32::MAX as i64))]
    pub sparks_per_hit: u32,
    #[arg(long, default_value_t = 100, value_parser = clap::value_parser!(u32).range(1..=i32::MAX as i64))]
    pub impacts_per_frame: u32,
    #[arg(long, default_value_t = 60, value_parser = clap::value_parser!(u32).range(0..=i32::MAX as i64))]
    pub warmup_frames: u32,
}

#[derive(Parser)]
pub struct TelemetryOptions {
    #[command(flatten)]
    pub editor: EditorOptions,
    #[arg(long, default_value_t = 7, value_parser = clap::value_parser!(u32).range(1..=100))]
    pub samples: u32,
    #[arg(long, default_value = ".local/benchmarks/level-telemetry")]
    pub output_dir: PathBuf,
}

#[derive(Args)]
pub struct CommandletOptions {
    #[command(flatten)]
    pub editor: EditorOptions,
    #[arg(long, value_parser = clap::value_parser!(u32).range(0..=i32::MAX as i64))]
    pub warmup: Option<u32>,
    #[arg(long, default_value_t = 100, value_parser = clap::value_parser!(u32).range(1..=i32::MAX as i64))]
    pub iterations: u32,
    #[arg(long)]
    pub output: Option<String>,
}

#[derive(Args)]
pub struct HeatmapOptions {
    #[command(flatten)]
    pub common: CommandletOptions,
    #[arg(long, default_value = "32,64,128,256,512")]
    pub resolutions: String,
}
#[derive(Args)]
pub struct RadarOptions {
    #[command(flatten)]
    pub common: CommandletOptions,
    #[arg(long, default_value = "32,128,256,512")]
    pub contact_counts: String,
}
#[derive(Args)]
pub struct ScatterOptions {
    #[command(flatten)]
    pub common: CommandletOptions,
    #[arg(long, default_value = "1,64,1024,16384,65536")]
    pub point_counts: String,
}

#[cfg(test)]
#[path = "../tests/cli/mod.rs"]
mod tests;
