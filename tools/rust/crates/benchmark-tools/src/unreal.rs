use crate::support::*;
use serde_json::Value;
use std::{
    fs,
    path::{Path, PathBuf},
    process::{Command, Output, Stdio},
    time::{Duration, Instant},
};

pub fn prepare(
    root: &Path,
    build_dir: Option<&str>,
    default_preset: &str,
    skip: bool,
) -> Result<Value> {
    let build = match build_dir {
        Some(directory) => absolute(root, directory)?,
        None => {
            if !skip {
                visible(root, "cmake", &["--preset", default_preset])?;
            }
            root.join("out/build").join(default_preset)
        }
    };
    if !skip {
        visible(
            root,
            "cmake",
            &["--build", &build.to_string_lossy(), "--target", "editor"],
        )?;
    }
    read_json(&build.join("unreal-paths.json"))
}

pub fn path(settings: &Value, key: &str) -> Result<PathBuf> {
    settings[key]
        .as_str()
        .filter(|value| !value.is_empty())
        .map(PathBuf::from)
        .ok_or_else(|| format!("Missing Unreal build path '{key}'").into())
}

// Bound engine measurements and retain output even when Unreal hangs or fails.
pub fn run_logged(command: &mut Command, log: &Path, deadline: Instant) -> Result<Output> {
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

fn automation_succeeded(log: &str) -> Result<()> {
    if log.contains("Found 0 automation tests based on")
        || log.contains("Test Completed. Result={Fail}")
        || log.contains("Test Completed. Result={Error}")
        || log.lines().any(|line| {
            line.split_once("TEST COMPLETE. EXIT CODE:")
                .and_then(|(_, value)| value.trim_start().split_whitespace().next())
                .and_then(|value| value.parse::<i32>().ok())
                .is_some_and(|code| code != 0)
        })
    {
        return Err("Unreal automation benchmark failed; see the measurement log".into());
    }
    Ok(())
}

fn measurement(operation: &str, args: &Args) -> Result<(Vec<String>, u64, bool)> {
    let mut command = Vec::new();
    let (timeout, automation) = match operation {
        "spark" => {
            command.push("-ExecCmds=Automation Now; RunTests StartsWith:SandboxBenchmarks.SparkBenchmark; Quit".into());
            command.push(format!(
                "-SandboxSparkBenchmarkSeconds={}",
                args.float("--seconds", 10.0, 0.01, 86400.0)?
            ));
            for (option, flag, default, min) in [
                ("--capacity", "Capacity", 50000, 1),
                ("--sparks-per-hit", "SparksPerHit", 96, 1),
                ("--impacts-per-frame", "ImpactsPerFrame", 100, 1),
                ("--warmup-frames", "WarmupFrames", 60, 0),
            ] {
                command.push(format!(
                    "-SandboxSparkBenchmark{flag}={}",
                    args.integer(option, default, min, i32::MAX as u32)?
                ));
            }
            command.extend(["-RenderOffscreen".into(), "-FullStdOutLogOutput".into()]);
            (1200, true)
        }
        "level-telemetry" => {
            command.push(
                "-ExecCmds=Automation Now; RunTests SandboxBenchmarks.LevelTelemetryStorage; Quit"
                    .into(),
            );
            command.push(format!(
                "-SandboxTelemetryBenchmarkSamples={}",
                args.integer("--samples", 7, 1, 100)?
            ));
            command.push("-FullStdOutLogOutput".into());
            (1200, true)
        }
        "entity-overlay" => {
            command.extend(
                [
                    "-run=EntityOverlayBenchmark",
                    "-RenderOffscreen",
                    "-AllowCommandletRendering",
                    "-DDC-ForceMemoryCache",
                ]
                .map(str::to_owned),
            );
            (1200, false)
        }
        "volume-heatmap-3d" => {
            command.extend(
                [
                    "-run=VolumeHeatmap3DBenchmark",
                    "-d3d12",
                    "-RenderOffscreen",
                    "-AllowCommandletRendering",
                ]
                .map(str::to_owned),
            );
            (1200, false)
        }
        _ => {
            let (name, option, flag, defaults, warmup) = match operation {
                "heatmap" => (
                    "HeatmapBenchmark",
                    "--resolutions",
                    "Resolutions",
                    "32,64,128,256,512",
                    10,
                ),
                "radar-3d" => (
                    "Radar3DBenchmark",
                    "--contact-counts",
                    "ContactCounts",
                    "32,128,256,512",
                    10,
                ),
                "scatter-3d" => (
                    "Scatter3DBenchmark",
                    "--point-counts",
                    "PointCounts",
                    "1,64,1024,16384,65536",
                    20,
                ),
                _ => return Err(format!("Unknown Unreal measurement '{operation}'").into()),
            };
            command.extend([
                format!("-run={name}"),
                format!("-{flag}={}", args.value(option, defaults)),
                format!(
                    "-Warmup={}",
                    args.integer("--warmup", warmup, 0, i32::MAX as u32)?
                ),
                format!(
                    "-Iterations={}",
                    args.integer("--iterations", 100, 1, i32::MAX as u32)?
                ),
                format!(
                    "-Output={}",
                    args.value("--output", &format!("Saved/Benchmarks/{name}.csv"))
                ),
                "-RenderOffscreen".into(),
                "-AllowCommandletRendering".into(),
            ]);
            (1200, false)
        }
    };
    Ok((command, timeout, automation))
}

pub fn execute(root: &Path, operation: &str, arguments: &[String]) -> Result<()> {
    let mut options = vec!["--build-dir", "--timeout-seconds"];
    options.extend(match operation {
        "spark" => vec![
            "--seconds",
            "--capacity",
            "--sparks-per-hit",
            "--impacts-per-frame",
            "--warmup-frames",
        ],
        "level-telemetry" => vec!["--samples", "--output-dir"],
        "heatmap" => vec!["--resolutions", "--warmup", "--iterations", "--output"],
        "radar-3d" => vec!["--contact-counts", "--warmup", "--iterations", "--output"],
        "scatter-3d" => vec!["--point-counts", "--warmup", "--iterations", "--output"],
        _ => vec![],
    });
    let args = Args::parse(arguments, &options, &["--skip-build"])?;
    let (arguments, timeout, automation) = measurement(operation, &args)?;
    let timeout = args.integer("--timeout-seconds", timeout as u32, 1, 86400)?;
    let settings = prepare(
        root,
        args.optional("--build-dir"),
        if automation {
            "development"
        } else {
            "debug-game"
        },
        args.flag("--skip-build"),
    )?;
    let mut command = Command::new(path(&settings, "editor_cmd")?);
    command
        .arg(path(&settings, "project")?)
        .args(arguments)
        .args(["-unattended", "-nop4", "-nosplash", "-nosound", "-stdout"])
        .current_dir(root);
    if automation {
        command.args([
            "-ddc=NoZenLocalFallback".into(),
            format!(
                "-LocalDataCachePath={}",
                path(&settings, "local_ddc")?.display()
            ),
        ]);
    }
    let log = if operation == "level-telemetry" {
        absolute(
            root,
            args.value("--output-dir", ".local/benchmarks/level-telemetry"),
        )?
        .join("telemetry-benchmark.log")
    } else {
        root.join(format!("Saved/Benchmarks/{operation}.log"))
    };
    let output = succeeded(run_logged(
        &mut command,
        &log,
        Instant::now() + Duration::from_secs(timeout.into()),
    )?)?;
    let text = String::from_utf8_lossy(&output.stdout);
    print!("{text}");
    if automation {
        automation_succeeded(&text)?;
    }
    Ok(())
}

#[cfg(test)]
#[path = "../tests/unreal/mod.rs"]
mod tests;
