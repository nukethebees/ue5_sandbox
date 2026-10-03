use crate::cli::{
    CommandletOptions, EditorOptions, HeatmapOptions, RadarOptions, ScatterOptions, SparkOptions,
    TelemetryOptions,
};
use crate::support::*;
use serde_json::Value;
use std::{
    fs,
    path::{Path, PathBuf},
    process::{Command, Output, Stdio},
    time::{Duration, Instant},
};

pub fn prepare_editor_build(
    root: &Path,
    build_dir: Option<&str>,
    default_preset: &str,
    skip: bool,
) -> Result<Value> {
    let build = match build_dir {
        Some(directory) => resolve_absolute_path(root, directory)?,
        None => {
            if !skip {
                run_process_inherited(root, "cmake", &["--preset", default_preset])?;
            }
            root.join("out/build").join(default_preset)
        }
    };
    if !skip {
        run_process_inherited(
            root,
            "cmake",
            &["--build", &build.to_string_lossy(), "--target", "editor"],
        )?;
    }
    read_json(&build.join("unreal-paths.json"))
}

pub fn read_build_path(settings: &Value, key: &str) -> Result<PathBuf> {
    settings[key]
        .as_str()
        .filter(|value| !value.is_empty())
        .map(PathBuf::from)
        .ok_or_else(|| format!("Missing Unreal build path '{key}'").into())
}

// Bound engine measurements and retain output even when Unreal hangs or fails.
pub fn run_editor_with_timeout(
    command: &mut Command,
    log: &Path,
    deadline: Instant,
) -> Result<Output> {
    if Instant::now() >= deadline {
        return Err(format!("Unreal benchmark timed out; log: {}", log.display()).into());
    }
    fs::create_dir_all(log.parent().ok_or("Log has no parent")?)?;
    let file = fs::File::create(log)?;
    let mut child = command
        .stdout(Stdio::from(file.try_clone()?))
        .stderr(Stdio::from(file))
        .spawn()?;
    let status = loop {
        if let Some(status) = child.try_wait()? {
            break status;
        }
        if Instant::now() >= deadline {
            child.kill()?;
            child.wait()?;
            return Err(format!("Unreal benchmark timed out; log: {}", log.display()).into());
        }
        std::thread::sleep(Duration::from_millis(100));
    };
    Ok(Output {
        status,
        stdout: fs::read(log)?,
        stderr: Vec::new(),
    })
}

fn validate_automation_log(log: &str) -> Result<()> {
    if log.contains("Found 0 automation tests based on")
        || log.contains("Test Completed. Result={Fail}")
        || log.contains("Test Completed. Result={Error}")
        || log.lines().any(|line| {
            line.split_once("TEST COMPLETE. EXIT CODE:")
                .and_then(|(_, value)| value.split_whitespace().next())
                .and_then(|value| value.parse::<i32>().ok())
                .is_some_and(|code| code != 0)
        })
    {
        return Err("Unreal automation benchmark failed; see the measurement log".into());
    }
    Ok(())
}

pub enum Benchmark {
    Spark(SparkOptions),
    Telemetry(TelemetryOptions),
    Heatmap(HeatmapOptions),
    Radar(RadarOptions),
    Scatter(ScatterOptions),
    VolumeHeatmap(EditorOptions),
    EntityOverlay(EditorOptions),
}

impl Benchmark {
    fn editor(&self) -> &EditorOptions {
        match self {
            Self::Spark(options) => &options.editor,
            Self::Telemetry(options) => &options.editor,
            Self::Heatmap(options) => &options.common.editor,
            Self::Radar(options) => &options.common.editor,
            Self::Scatter(options) => &options.common.editor,
            Self::VolumeHeatmap(options) | Self::EntityOverlay(options) => options,
        }
    }

    fn name(&self) -> &'static str {
        match self {
            Self::Spark(_) => "spark",
            Self::Telemetry(_) => "level-telemetry",
            Self::Heatmap(_) => "heatmap",
            Self::Radar(_) => "radar-3d",
            Self::Scatter(_) => "scatter-3d",
            Self::VolumeHeatmap(_) => "volume-heatmap-3d",
            Self::EntityOverlay(_) => "entity-overlay",
        }
    }
}

fn commandlet_arguments(
    name: &str,
    flag: &str,
    values: &str,
    options: &CommandletOptions,
    warmup: u32,
) -> Vec<String> {
    vec![
        format!("-run={name}"),
        format!("-{flag}={values}"),
        format!("-Warmup={}", options.warmup.unwrap_or(warmup)),
        format!("-Iterations={}", options.iterations),
        format!(
            "-Output={}",
            options
                .output
                .as_deref()
                .unwrap_or(&format!("Saved/Benchmarks/{name}.csv"))
        ),
        "-RenderOffscreen".into(),
        "-AllowCommandletRendering".into(),
    ]
}

fn measurement_arguments(benchmark: &Benchmark) -> (Vec<String>, bool) {
    match benchmark {
        Benchmark::Spark(options) => {
            let mut command = vec![
                "-ExecCmds=Automation Now; RunTests StartsWith:SandboxBenchmarks.SparkBenchmark; Quit".into(),
                format!("-SandboxSparkBenchmarkSeconds={}", options.seconds),
                "-RenderOffscreen".into(),
                "-FullStdOutLogOutput".into(),
            ];
            for (flag, value) in [
                ("Capacity", options.capacity),
                ("SparksPerHit", options.sparks_per_hit),
                ("ImpactsPerFrame", options.impacts_per_frame),
                ("WarmupFrames", options.warmup_frames),
            ] {
                command.push(format!("-SandboxSparkBenchmark{flag}={value}"));
            }
            (command, true)
        }
        Benchmark::Telemetry(options) => (
            vec![
                "-ExecCmds=Automation Now; RunTests SandboxBenchmarks.LevelTelemetryStorage; Quit"
                    .into(),
                format!("-SandboxTelemetryBenchmarkSamples={}", options.samples),
                "-FullStdOutLogOutput".into(),
            ],
            true,
        ),
        Benchmark::EntityOverlay(_) => (
            [
                "-run=EntityOverlayBenchmark",
                "-RenderOffscreen",
                "-AllowCommandletRendering",
                "-DDC-ForceMemoryCache",
            ]
            .map(str::to_owned)
            .to_vec(),
            false,
        ),
        Benchmark::VolumeHeatmap(_) => (
            [
                "-run=VolumeHeatmap3DBenchmark",
                "-d3d12",
                "-RenderOffscreen",
                "-AllowCommandletRendering",
            ]
            .map(str::to_owned)
            .to_vec(),
            false,
        ),
        Benchmark::Heatmap(options) => (
            commandlet_arguments(
                "HeatmapBenchmark",
                "Resolutions",
                &options.resolutions,
                &options.common,
                10,
            ),
            false,
        ),
        Benchmark::Radar(options) => (
            commandlet_arguments(
                "Radar3DBenchmark",
                "ContactCounts",
                &options.contact_counts,
                &options.common,
                10,
            ),
            false,
        ),
        Benchmark::Scatter(options) => (
            commandlet_arguments(
                "Scatter3DBenchmark",
                "PointCounts",
                &options.point_counts,
                &options.common,
                20,
            ),
            false,
        ),
    }
}

pub fn run_unreal_benchmark(root: &Path, benchmark: Benchmark) -> Result<()> {
    let (arguments, automation) = measurement_arguments(&benchmark);
    let options = benchmark.editor();
    let settings = prepare_editor_build(
        root,
        options.build_dir.as_deref(),
        if automation {
            "development"
        } else {
            "debug-game"
        },
        options.skip_build,
    )?;
    let mut command = Command::new(read_build_path(&settings, "editor_cmd")?);
    command
        .arg(read_build_path(&settings, "project")?)
        .args(arguments)
        .args(["-unattended", "-nop4", "-nosplash", "-nosound", "-stdout"])
        .current_dir(root);
    if automation {
        command.args([
            "-ddc=NoZenLocalFallback".into(),
            format!(
                "-LocalDataCachePath={}",
                read_build_path(&settings, "local_ddc")?.display()
            ),
        ]);
    }
    let log = match &benchmark {
        Benchmark::Telemetry(options) => {
            resolve_absolute_path(root, &options.output_dir)?.join("telemetry-benchmark.log")
        }
        _ => root.join(format!("Saved/Benchmarks/{}.log", benchmark.name())),
    };
    let output = require_process_success(run_editor_with_timeout(
        &mut command,
        &log,
        Instant::now() + Duration::from_secs(options.timeout_seconds.into()),
    )?)?;
    let text = String::from_utf8_lossy(&output.stdout);
    print!("{text}");
    if automation {
        validate_automation_log(&text)?;
    }
    Ok(())
}

#[cfg(test)]
#[path = "../tests/unreal/mod.rs"]
mod tests;
