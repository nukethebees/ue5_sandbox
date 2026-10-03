use crate::cli::{FighterOptions, FrameMemoryOptions, SimulationOptions};
use crate::support::*;
use serde_json::Value;
use std::path::Path;

pub fn run_simulation_benchmark(root: &Path, args: &SimulationOptions) -> Result<String> {
    capture_simulation_output(root, args, None)
}

pub fn run_simulation_benchmark_with_logs(
    root: &Path,
    args: &SimulationOptions,
    directory: &Path,
) -> Result<String> {
    capture_simulation_output(root, args, Some(directory))
}

fn capture_simulation_output(
    root: &Path,
    parsed: &SimulationOptions,
    directory: Option<&Path>,
) -> Result<String> {
    let level = resolve_absolute_path(&std::env::current_dir()?, &parsed.level)?;
    if !level.is_file() {
        return Err(format!("The level path does not exist: '{}'.", level.display()).into());
    }
    let seconds = parsed.seconds;
    let speed = parsed.game_speed;
    let warmup = parsed.warmup_seconds;
    let timeout = parsed.saturation_timeout_seconds;
    let mut command = vec![
        "--level".into(),
        level.to_string_lossy().into_owned(),
        "--seconds".into(),
        seconds.to_string(),
        "--game-speed".into(),
        speed.to_string(),
    ];
    if parsed.telemetry {
        command.push("--telemetry".into());
    }
    if let Some(cap) = parsed.fighter_stress_cap {
        command.extend(["--fighter-stress-cap".into(), cap.to_string()]);
    }
    if let Some(caps) = &parsed.fighter_stress_caps {
        command.push("--fighter-stress-caps".into());
        command.extend(parse_fighter_caps(caps)?.iter().map(u32::to_string));
    }
    if parsed.fighter_stress_cap.is_some() || parsed.fighter_stress_caps.is_some() {
        command.extend([
            "--warmup-seconds".into(),
            warmup.to_string(),
            "--saturation-timeout-seconds".into(),
            timeout.to_string(),
        ]);
    }
    let build = parsed.build_preset.as_str();
    let configure = match build {
        "native-simulation-benchmark" | "frame-memory-level-benchmark" => "native-benchmark",
        _ => build,
    };
    if !parsed.skip_build {
        run_process_inherited(root, "cmake", &["--preset", configure])?;
        run_process_inherited(root, "cmake", &["--build", "--preset", build])?;
    }
    let executable = root
        .join("out/build")
        .join(configure)
        .join("bin/native-simulation-benchmark.exe");
    if !executable.is_file() {
        return Err(format!(
            "The native simulation benchmark executable was not built: '{}'.",
            executable.display()
        )
        .into());
    }
    let result = capture_process_output(
        std::process::Command::new(executable)
            .args(&command)
            .current_dir(root),
    )?;
    if let Some(directory) = directory {
        write_text(&directory.join("stdout.jsonl"), &result.stdout)?;
        write_text(&directory.join("stderr.log"), &result.stderr)?;
    }
    let result = require_process_success(result)?;
    eprint!("{}", String::from_utf8_lossy(&result.stderr));
    Ok(String::from_utf8_lossy(&result.stdout).into_owned())
}

pub(crate) fn parse_fighter_caps(text: &str) -> Result<Vec<u32>> {
    let mut values = Vec::new();
    for item in text.split(',').map(str::trim) {
        let cap = item
            .parse::<u32>()
            .ok()
            .filter(|n| *n > 0 && item.bytes().all(|c| c.is_ascii_digit()))
            .ok_or("Fighter caps must contain unique positive 32-bit integers.")?;
        if values.contains(&cap) {
            return Err("Fighter caps must contain unique positive 32-bit integers.".into());
        }
        values.push(cap);
    }
    Ok(values)
}

pub fn run_fighter_benchmark(root: &Path, parsed: &FighterOptions) -> Result<()> {
    let caps = parse_fighter_caps(&parsed.fighter_caps)?;
    let seconds = parsed.seconds;
    let default_output = format!(
        ".local/benchmarks/fighter-simulation/{}",
        chrono::Local::now().format("%Y%m%d-%H%M%S")
    );
    let output = resolve_absolute_path(
        root,
        parsed
            .output_dir
            .as_deref()
            .unwrap_or(Path::new(&default_output)),
    )?;
    let command = SimulationOptions {
        level: root.join("LevelScripts/FighterSchedulingBenchmark.scm"),
        seconds,
        game_speed: 1,
        fighter_stress_cap: None,
        fighter_stress_caps: Some(parsed.fighter_caps.clone()),
        warmup_seconds: parsed.warmup_seconds,
        saturation_timeout_seconds: parsed.saturation_timeout_seconds,
        build_preset: "native-simulation-benchmark".into(),
        telemetry: false,
        skip_build: parsed.skip_build,
    };
    let results = parse_json_lines(&run_simulation_benchmark(root, &command)?)?;
    if results.len() != caps.len() {
        return Err(format!(
            "Expected {} benchmark JSON results, found {}.",
            caps.len(),
            results.len()
        )
        .into());
    }
    for (result, cap) in results.iter().zip(&caps) {
        validate_fighter_result(result, *cap, (seconds * 60.0).ceil() as u64)?;
    }
    write_fighter_reports(&output, &results, &caps)
}

fn write_fighter_reports(output: &Path, results: &[Value], caps: &[u32]) -> Result<()> {
    let mut rows = vec![
        [
            "fighter_cap",
            "steady_state_fighters",
            "measured_ticks",
            "elapsed_seconds",
            "mean_tick_us",
            "median_tick_us",
            "p95_tick_us",
            "p99_tick_us",
            "ticks_per_second",
            "realtime_factor",
            "lasers_spawned",
        ]
        .map(str::to_owned)
        .to_vec(),
    ];
    for (result, cap) in results.iter().zip(caps) {
        let mut row = vec![cap.to_string()];
        for pointer in [
            "/fighter_stress/steady_state_fighters",
            "/workload/measured_ticks",
            "/timing/elapsed_seconds",
            "/timing/mean_tick_microseconds",
            "/timing/median_tick_microseconds",
            "/timing/p95_tick_microseconds",
            "/timing/p99_tick_microseconds",
            "/timing/ticks_per_second",
            "/timing/realtime_factor",
            "/fighter_stress/lasers_spawned_during_measurement",
        ] {
            row.push(
                result
                    .pointer(pointer)
                    .ok_or("Missing summary field")?
                    .to_string(),
            );
        }
        rows.push(row);
    }
    write_json(&output.join("results.json"), &results)?;
    write_csv(&output.join("summary.csv"), &rows)?;
    crate::plots::automatic(|| {
        crate::plots::plot_fighters(&serde_json::to_value(&results)?, &output.join("plots"))
    });
    println!("Results written to {}", output.display());
    Ok(())
}

pub fn validate_fighter_result(result: &Value, cap: u32, ticks: u64) -> Result<()> {
    let valid = result["level"]["id"] == "fighter-scheduling-benchmark"
        && result["fighter_stress"]["enabled"] == true
        && result["workload"]["measured_ticks"].as_u64() == Some(ticks)
        && [
            "configured_cap",
            "steady_state_fighters",
            "minimum_measured_fighters",
            "maximum_measured_fighters",
        ]
        .iter()
        .all(|key| result["fighter_stress"][key].as_u64() == Some(cap.into()))
        && result["fighter_stress"]["fighter_spawns_during_measurement"].as_u64() == Some(0)
        && result["fighter_stress"]["task_counts"]["attack"].as_u64() == Some(cap.into())
        && read_finite_number(result, "/fighter_stress/lasers_spawned_during_measurement")? > 0.0
        && result["memory"]["frame_overflow_count"].as_u64() == Some(0);
    if !valid {
        return Err(format!("Fighter benchmark validation failed for cap {cap}.").into());
    }
    for metric in [
        "elapsed_seconds",
        "mean_tick_microseconds",
        "median_tick_microseconds",
        "p95_tick_microseconds",
        "p99_tick_microseconds",
        "ticks_per_second",
        "realtime_factor",
    ] {
        read_finite_number(result, &format!("/timing/{metric}"))?;
    }
    Ok(())
}

pub fn run_frame_memory_benchmark(root: &Path, parsed: &FrameMemoryOptions) -> Result<()> {
    let mut command = frame_memory_options(root, parsed.seconds);
    command.skip_build = parsed.skip_build;
    let results = parse_json_lines(&run_simulation_benchmark(root, &command)?)?;
    if results.len() != 1 {
        return Err("Expected one native simulation benchmark JSON result.".into());
    }
    validate_frame_memory_result(&results[0])?;
    println!("{}", results[0]);
    Ok(())
}

pub fn frame_memory_options(root: &Path, seconds: f64) -> SimulationOptions {
    SimulationOptions {
        level: root.join("LevelScripts/Benchmarks/Batch_benchmark.scm"),
        seconds,
        game_speed: 100,
        build_preset: "frame-memory-level-benchmark".into(),
        fighter_stress_cap: None,
        fighter_stress_caps: None,
        warmup_seconds: 5.0,
        saturation_timeout_seconds: 60.0,
        telemetry: false,
        skip_build: false,
    }
}

pub fn validate_frame_memory_result(result: &Value) -> Result<()> {
    let requested = result["workload"]["requested_ticks"]
        .as_u64()
        .ok_or("Missing requested_ticks")?;
    if result["level"]["id"] != "batch-benchmark"
        || result["workload"]["completed_ticks"].as_u64() != Some(requested)
        || result["workload"]["game_speed"].as_u64() != Some(100)
        || result["workload"]["advance_calls"].as_u64() != Some(requested)
        || result["memory"]["frame_overflow_count"].as_u64() != Some(0)
        || read_finite_number(result, "/memory/frame_peak_claimed_bytes")? <= 0.0
        || read_finite_number(result, "/final_state/peak_fighters")? <= 0.0
    {
        return Err("Frame-memory benchmark validation failed.".into());
    }
    Ok(())
}

#[cfg(test)]
#[path = "../tests/native/mod.rs"]
mod tests;
