use crate::support::*;
use serde_json::Value;
use std::path::Path;

pub fn run_simulation_benchmark(root: &Path, args: &[String]) -> Result<String> {
    let parsed = Args::parse_command_line(
        args,
        &[
            "--level",
            "--seconds",
            "--game-speed",
            "--fighter-stress-cap",
            "--fighter-stress-caps",
            "--warmup-seconds",
            "--saturation-timeout-seconds",
            "--build-preset",
        ],
        &["--telemetry", "--skip-build"],
    )?;
    let level = resolve_absolute_path(&std::env::current_dir()?, parsed.required("--level")?)?;
    if !level.is_file() {
        return Err(format!("The level path does not exist: '{}'.", level.display()).into());
    }
    parsed.required("--seconds")?;
    let seconds = parsed.float("--seconds", 0.0, f64::MIN_POSITIVE, f64::MAX)?;
    let speed = parsed.integer("--game-speed", 1, 1, u32::MAX)?;
    let warmup = parsed.float("--warmup-seconds", 5.0, 0.0, 86400.0)?;
    let timeout = parsed.float("--saturation-timeout-seconds", 60.0, 0.1, 86400.0)?;
    if parsed.flag("--fighter-stress-cap") && parsed.flag("--fighter-stress-caps") {
        return Err(
            "--fighter-stress-cap and --fighter-stress-caps are mutually exclusive.".into(),
        );
    }
    let mut command = vec![
        "--level".into(),
        level.to_string_lossy().into_owned(),
        "--seconds".into(),
        seconds.to_string(),
        "--game-speed".into(),
        speed.to_string(),
    ];
    if parsed.flag("--telemetry") {
        command.push("--telemetry".into());
    }
    if parsed.flag("--fighter-stress-cap") {
        let cap = parsed.integer("--fighter-stress-cap", 1, 1, u32::MAX)?;
        command.extend(["--fighter-stress-cap".into(), cap.to_string()]);
    }
    if parsed.flag("--fighter-stress-caps") {
        command.push("--fighter-stress-caps".into());
        command.extend(
            parse_fighter_caps(parsed.required("--fighter-stress-caps")?)?
                .iter()
                .map(u32::to_string),
        );
    }
    if parsed.flag("--fighter-stress-cap") || parsed.flag("--fighter-stress-caps") {
        command.extend([
            "--warmup-seconds".into(),
            warmup.to_string(),
            "--saturation-timeout-seconds".into(),
            timeout.to_string(),
        ]);
    }
    let build = parsed.value("--build-preset", "native-simulation-benchmark");
    let configure = match build {
        "native-simulation-benchmark" | "frame-memory-level-benchmark" => "native-benchmark",
        _ => build,
    };
    if !parsed.flag("--skip-build") {
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

pub fn run_fighter_benchmark(root: &Path, args: &[String]) -> Result<()> {
    let parsed = Args::parse_command_line(
        args,
        &[
            "--fighter-caps",
            "--seconds",
            "--warmup-seconds",
            "--saturation-timeout-seconds",
            "--output-dir",
        ],
        &["--skip-build"],
    )?;
    let caps = parse_fighter_caps(parsed.value("--fighter-caps", "2000,4000"))?;
    let seconds = parsed.float("--seconds", 10.0, 0.1, 86400.0)?;
    let warmup = parsed.float("--warmup-seconds", 5.0, 0.0, 86400.0)?;
    let timeout = parsed.float("--saturation-timeout-seconds", 60.0, 0.1, 86400.0)?;
    let default_output = format!(
        ".local/benchmarks/fighter-simulation/{}",
        chrono::Local::now().format("%Y%m%d-%H%M%S")
    );
    let output = resolve_absolute_path(root, parsed.value("--output-dir", &default_output))?;
    let mut command = vec![
        "--level".into(),
        root.join("LevelScripts/FighterSchedulingBenchmark.scm")
            .to_string_lossy()
            .into_owned(),
        "--seconds".into(),
        seconds.to_string(),
        "--fighter-stress-caps".into(),
        caps.iter()
            .map(u32::to_string)
            .collect::<Vec<_>>()
            .join(","),
        "--warmup-seconds".into(),
        warmup.to_string(),
        "--saturation-timeout-seconds".into(),
        timeout.to_string(),
    ];
    if parsed.flag("--skip-build") {
        command.push("--skip-build".into());
    }
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

pub fn run_frame_memory_benchmark(root: &Path, args: &[String]) -> Result<()> {
    let parsed = Args::parse_command_line(args, &["--seconds"], &["--skip-build"])?;
    let seconds = parsed.float("--seconds", 20.0, 0.1, 86400.0)?;
    let mut command = frame_memory_arguments(root, seconds);
    if parsed.flag("--skip-build") {
        command.push("--skip-build".into());
    }
    let results = parse_json_lines(&run_simulation_benchmark(root, &command)?)?;
    if results.len() != 1 {
        return Err("Expected one native simulation benchmark JSON result.".into());
    }
    validate_frame_memory_result(&results[0])?;
    println!("{}", results[0]);
    Ok(())
}

pub fn frame_memory_arguments(root: &Path, seconds: f64) -> Vec<String> {
    vec![
        "--level".into(),
        root.join("LevelScripts/Benchmarks/Batch_benchmark.scm")
            .to_string_lossy()
            .into_owned(),
        "--seconds".into(),
        seconds.to_string(),
        "--game-speed".into(),
        "100".into(),
        "--build-preset".into(),
        "frame-memory-level-benchmark".into(),
    ]
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
