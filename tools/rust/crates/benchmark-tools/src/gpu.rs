use crate::{results::Summary, support::*};
use serde::Serialize;
use serde_json::json;
use std::{
    collections::BTreeMap,
    fs,
    path::{Path, PathBuf},
    process::Command,
};

const METRICS: [(&str, &str); 8] = [
    ("game_thread_ms", "GameThreadTime"),
    ("render_thread_ms", "RenderThreadTime"),
    ("gpu_ms", "GPUTime"),
    ("translucency_gpu_ms", "GPU/Translucency"),
    (
        "starfield_submit_cpu_ms",
        "Exclusive/AllWorkers/GpuStarfieldSubmit",
    ),
    ("rhi_draw_calls", "RHI/DrawCalls"),
    ("translucency_draw_calls", "DrawCall/Translucency"),
    ("primitives_drawn", "RHI/PrimitivesDrawn"),
];

#[derive(Clone, Debug, Serialize, PartialEq)]
#[serde(rename_all = "camelCase")]
struct Configuration {
    width: u32,
    height: u32,
    size_multiplier: f64,
}
impl Configuration {
    fn name(&self) -> String {
        format!(
            "{}x{}_size{}",
            self.width,
            self.height,
            self.size_multiplier.to_string().replace('.', "p")
        )
    }
}
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct Request {
    editor: PathBuf,
    project: PathBuf,
    output: PathBuf,
    counts: Vec<u32>,
    configurations: Vec<Configuration>,
    camera_modes: Vec<bool>,
    warmup_frames: u32,
    capture_frames: u32,
    repeats: u32,
    trim_frames: u32,
}
impl Request {
    fn parse(root: &Path, args: &[String]) -> Result<Self> {
        let args = Args::parse(
            args,
            &[
                "--editor",
                "--project",
                "--output",
                "--counts",
                "--resolutions",
                "--size-multipliers",
                "--camera-modes",
                "--warmup-frames",
                "--capture-frames",
                "--repeats",
                "--trim-frames",
            ],
            &[],
        )?;
        let mut counts = Vec::new();
        for item in args.value("--counts", "10000,100000,1000000").split(',') {
            let count = item.trim().parse::<u32>()?;
            if !(1..=1_000_000).contains(&count) || counts.contains(&count) {
                return Err("--counts must contain unique integers in 1..1000000.".into());
            }
            counts.push(count);
        }
        let mut resolutions = Vec::new();
        for item in args.value("--resolutions", "1280x720").split(',') {
            let (width, height) = item
                .trim()
                .split_once(['x', 'X'])
                .ok_or("--resolutions requires WIDTHxHEIGHT.")?;
            let pair = (width.parse::<u32>()?, height.parse::<u32>()?);
            if !(320..=7680).contains(&pair.0) || !(200..=4320).contains(&pair.1) {
                return Err("Resolution must be between 320x200 and 7680x4320.".into());
            }
            if !resolutions.contains(&pair) {
                resolutions.push(pair);
            }
        }
        let mut sizes = Vec::new();
        for item in args.value("--size-multipliers", "1").split(',') {
            let size = item.trim().parse::<f64>()?;
            if !size.is_finite() || !(0.0..=100.0).contains(&size) {
                return Err("--size-multipliers must contain finite values from 0 to 100.".into());
            }
            if !sizes.contains(&size) {
                sizes.push(size);
            }
        }
        let mut camera_modes = Vec::new();
        for item in args.value("--camera-modes", "stationary,moving").split(',') {
            let moving = match item.trim().to_lowercase().as_str() {
                "stationary" => false,
                "moving" => true,
                _ => return Err("--camera-modes requires stationary and/or moving.".into()),
            };
            if !camera_modes.contains(&moving) {
                camera_modes.push(moving);
            }
        }
        Ok(Self {
            editor: absolute(&std::env::current_dir()?, args.required("--editor")?)?,
            project: absolute(&std::env::current_dir()?, args.required("--project")?)?,
            output: absolute(root, args.required("--output")?)?,
            counts,
            configurations: resolutions
                .iter()
                .flat_map(|&(width, height)| {
                    sizes.iter().map(move |&size_multiplier| Configuration {
                        width,
                        height,
                        size_multiplier,
                    })
                })
                .collect(),
            camera_modes,
            warmup_frames: args.integer("--warmup-frames", 60, 1, i32::MAX as u32)?,
            capture_frames: args.integer("--capture-frames", 180, 1, i32::MAX as u32)?,
            repeats: args.integer("--repeats", 3, 1, i32::MAX as u32)?,
            trim_frames: args.integer("--trim-frames", 10, 1, i32::MAX as u32)?,
        })
    }
    fn arguments(&self, config: &Configuration, raw: &Path) -> Vec<String> {
        let mut args = vec![self.project.to_string_lossy().into_owned()];
        args.extend(
            [
                "/SandboxShaders/Showcase/SandboxShaders_Showcase",
                "-game",
                "-RenderOffscreen",
                "-unattended",
                "-nop4",
                "-nosplash",
                "-nosound",
                "-stdout",
            ]
            .map(str::to_owned),
        );
        args.extend([
            format!("-ResX={}", config.width),
            format!("-ResY={}", config.height),
        ]);
        args.extend(
            [
                "-ForceRes",
                "-windowed",
                "-benchmark",
                "-deterministic",
                "-fps=60",
                "-csvGpuStats",
                "-GpuStarfieldBenchmark",
            ]
            .map(str::to_owned),
        );
        args.extend([
            format!(
                "-GpuStarfieldBenchmarkCounts={}",
                self.counts
                    .iter()
                    .map(u32::to_string)
                    .collect::<Vec<_>>()
                    .join(",")
            ),
            format!(
                "-GpuStarfieldBenchmarkStarSizeMultiplier={}",
                config.size_multiplier
            ),
            format!(
                "-GpuStarfieldBenchmarkCameraModes={}",
                self.camera_modes
                    .iter()
                    .map(|&moving| camera(moving))
                    .collect::<Vec<_>>()
                    .join(",")
            ),
            format!("-GpuStarfieldBenchmarkWarmupFrames={}", self.warmup_frames),
            format!(
                "-GpuStarfieldBenchmarkCaptureFrames={}",
                self.capture_frames
            ),
            format!("-GpuStarfieldBenchmarkRepeats={}", self.repeats),
            format!("-GpuStarfieldBenchmarkOutput={}", raw.display()),
            "-ExecCmds=r.VSync 0;r.ScreenPercentage 100;r.DynamicRes.OperationMode 0".into(),
        ]);
        args
    }
}
fn camera(moving: bool) -> &'static str {
    if moving { "moving" } else { "stationary" }
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
struct Capture {
    configuration: Configuration,
    star_count: u32,
    repeat: u32,
    enabled: bool,
    moving: bool,
    frame_count: usize,
    medians: BTreeMap<String, f64>,
    minima: BTreeMap<String, f64>,
    maxima: BTreeMap<String, f64>,
}
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct Delta {
    configuration: Configuration,
    star_count: u32,
    moving: bool,
    metric: String,
    disabled_median: f64,
    enabled_median: f64,
    delta_median: f64,
    delta_min: f64,
    delta_max: f64,
}

fn read_capture(
    path: &Path,
    configuration: &Configuration,
    count: u32,
    repeat: u32,
    enabled: bool,
    moving: bool,
    trim: u32,
) -> Result<Capture> {
    let text = fs::read_to_string(path)?;
    let mut lines = text.trim_start_matches('\u{feff}').lines();
    let columns = parse_csv(lines.next().ok_or("Capture is empty.")?)?;
    let indices: BTreeMap<_, _> = METRICS
        .iter()
        .filter_map(|&(name, column)| columns.iter().position(|c| c == column).map(|i| (name, i)))
        .collect();
    let gt = *indices
        .get("game_thread_ms")
        .ok_or("Capture has no GameThreadTime column.")?;
    let mut values: BTreeMap<&str, Vec<f64>> =
        METRICS.iter().map(|(k, _)| (*k, Vec::new())).collect();
    let (mut width, mut height) = (0, 0);
    for line in lines {
        let row = parse_csv(line)?;
        for pair in row.windows(2) {
            if pair[0] == "[systemresolution.resx]" {
                if let Ok(v) = pair[1].parse() {
                    width = v;
                }
            }
            if pair[0] == "[systemresolution.resy]" {
                if let Ok(v) = pair[1].parse() {
                    height = v;
                }
            }
        }
        let numeric = |index| {
            row.get(index)
                .and_then(|s: &String| s.parse::<f64>().ok())
                .filter(|v| v.is_finite())
        };
        if numeric(gt).is_none() {
            continue;
        }
        for (&name, &index) in &indices {
            if let Some(value) = numeric(index) {
                values.get_mut(name).unwrap().push(value);
            }
        }
    }
    let frame_count = values["game_thread_ms"].len();
    let trim = trim as usize;
    if frame_count <= trim.saturating_mul(2) {
        return Err(format!(
            "Capture has only {frame_count} frames: '{}'.",
            path.display()
        )
        .into());
    }
    if (width, height) != (configuration.width, configuration.height) {
        return Err(format!(
            "Capture rendered at {width}x{height}, expected {}x{}.",
            configuration.width, configuration.height
        )
        .into());
    }
    let mut capture = Capture {
        configuration: configuration.clone(),
        star_count: count,
        repeat,
        enabled,
        moving,
        frame_count,
        medians: BTreeMap::new(),
        minima: BTreeMap::new(),
        maxima: BTreeMap::new(),
    };
    for (name, samples) in values {
        let summary = if samples.len() > 2 * trim {
            Summary::across(samples[trim..samples.len() - trim].iter().copied())?
        } else {
            Summary {
                samples: 0,
                min: 0.0,
                median: 0.0,
                p95: 0.0,
                max: 0.0,
            }
        };
        capture.medians.insert(name.into(), summary.median);
        capture.minima.insert(name.into(), summary.min);
        capture.maxima.insert(name.into(), summary.max);
    }
    Ok(capture)
}

fn deltas(request: &Request, captures: &[Capture]) -> Result<Vec<Delta>> {
    let mut result = Vec::new();
    for configuration in &request.configurations {
        for &count in &request.counts {
            for &moving in &request.camera_modes {
                for (metric, _) in METRICS {
                    let mut disabled = Vec::new();
                    let mut enabled = Vec::new();
                    for repeat in 1..=request.repeats {
                        for (state, values) in [(false, &mut disabled), (true, &mut enabled)] {
                            let found: Vec<_> = captures
                                .iter()
                                .filter(|c| {
                                    c.configuration == *configuration
                                        && c.star_count == count
                                        && c.repeat == repeat
                                        && c.enabled == state
                                        && c.moving == moving
                                })
                                .collect();
                            if found.len() != 1 {
                                return Err(format!("Missing or duplicate GPU capture: {}, {count} stars, repeat {repeat}.",configuration.name()).into());
                            }
                            values.push(
                                *found[0]
                                    .medians
                                    .get(metric)
                                    .filter(|v| v.is_finite())
                                    .ok_or("Missing finite GPU capture metric.")?,
                            );
                        }
                    }
                    let delta = Summary::across(enabled.iter().zip(&disabled).map(|(b, a)| b - a))?;
                    result.push(Delta {
                        configuration: configuration.clone(),
                        star_count: count,
                        moving,
                        metric: metric.into(),
                        disabled_median: Summary::across(disabled)?.median,
                        enabled_median: Summary::across(enabled)?.median,
                        delta_median: delta.median,
                        delta_min: delta.min,
                        delta_max: delta.max,
                    });
                }
            }
        }
    }
    Ok(result)
}
fn validate(captures: &[Capture], deltas: &[Delta]) -> Result<()> {
    for delta in deltas {
        let expected = (delta.star_count * 2 + 2) as f64;
        let valid = match delta.metric.as_str() {
            "translucency_draw_calls" => (1.5..=2.5).contains(&delta.delta_median),
            "primitives_drawn" => {
                (delta.delta_median - expected).abs() <= (expected * 0.02).max(4.0)
            }
            "starfield_submit_cpu_ms" => delta.enabled_median > 0.0,
            _ => true,
        };
        if !valid {
            return Err(format!(
                "Benchmark validation failed: {} for {}, {} stars.",
                delta.metric,
                delta.configuration.name(),
                delta.star_count
            )
            .into());
        }
    }
    for capture in captures.iter().filter(|c| c.moving && c.enabled) {
        if capture
            .minima
            .get("primitives_drawn")
            .is_none_or(|v| *v < (capture.star_count * 2 + 2) as f64)
        {
            return Err(
                format!("Moving stars disappeared during repeat {}.", capture.repeat).into(),
            );
        }
    }
    Ok(())
}
fn write_outputs(
    directory: &Path,
    request: &Request,
    captures: &[Capture],
    deltas: &[Delta],
) -> Result<()> {
    write_json(
        &directory.join("GpuStarfieldBenchmark.json"),
        &json!({"schemaVersion":1,"request":request,"captures":captures,"deltas":deltas}),
    )?;
    let mut rows = vec![
        [
            "width",
            "height",
            "star_size_multiplier",
            "star_count",
            "camera_motion",
            "metric",
            "disabled_median",
            "enabled_median",
            "delta_median",
            "delta_min",
            "delta_max",
        ]
        .map(str::to_owned)
        .to_vec(),
    ];
    let mut report = String::from(
        "# GPU starfield isolated A/B benchmark\n\nValues are medians of paired enabled-minus-disabled captures. Ranges are the minimum and maximum paired delta across repeats. CPU totals remain whole-frame deltas; the submission scope measures GetDynamicMeshElements on a worker.\n",
    );
    if request.camera_modes.contains(&true) {
        report.push_str(
            "\nMoving captures translate the camera 10,000 km along a deterministic curved path.\n",
        );
    }
    report.push_str("\nPaired delta ranges:\n\n| Resolution | Size | Stars | Camera | Metric | Disabled median | Enabled median | Delta median | Delta range |\n|---|---:|---:|---|---|---:|---:|---:|---|\n");
    for d in deltas {
        rows.push(vec![
            d.configuration.width.to_string(),
            d.configuration.height.to_string(),
            d.configuration.size_multiplier.to_string(),
            d.star_count.to_string(),
            camera(d.moving).into(),
            d.metric.clone(),
            d.disabled_median.to_string(),
            d.enabled_median.to_string(),
            d.delta_median.to_string(),
            d.delta_min.to_string(),
            d.delta_max.to_string(),
        ]);
        report.push_str(&format!(
            "| {}x{} | {} | {} | {} | {} | {:.6} | {:.6} | {:.6} | [{:.6}, {:.6}] |\n",
            d.configuration.width,
            d.configuration.height,
            d.configuration.size_multiplier,
            d.star_count,
            camera(d.moving),
            d.metric,
            d.disabled_median,
            d.enabled_median,
            d.delta_median,
            d.delta_min,
            d.delta_max
        ));
    }
    write_csv(&directory.join("GpuStarfieldBenchmark.csv"), &rows)?;
    write_text(&directory.join("GpuStarfieldBenchmark.md"), &report)?;
    print!("{report}");
    Ok(())
}
pub fn execute(root: &Path, args: &[String]) -> Result<()> {
    let request = Request::parse(root, args)?;
    let directory = request
        .output
        .join(chrono::Local::now().format("%Y%m%d_%H%M%S").to_string());
    fs::create_dir_all(&request.output)?;
    fs::create_dir(&directory)?;
    write_text(
        &request.output.join("latest.txt"),
        format!("{}\n", directory.display()),
    )?;
    let mut captures = Vec::new();
    for configuration in &request.configurations {
        let config_dir = directory.join(configuration.name());
        let raw = config_dir.join("raw");
        fs::create_dir_all(&raw)?;
        println!("Running {}...", configuration.name());
        let process = logged(
            Command::new(&request.editor)
                .args(request.arguments(configuration, &raw))
                .current_dir(request.project.parent().ok_or("Project has no parent.")?),
            &config_dir.join("unreal.log"),
        )?;
        succeeded(process)?;
        for &count in &request.counts {
            for repeat in 1..=request.repeats {
                for &moving in &request.camera_modes {
                    for enabled in [false, true] {
                        let path = raw.join(format!(
                            "gpu_starfield_{count}_{}_{}_r{repeat}.csv",
                            camera(moving),
                            if enabled { "enabled" } else { "disabled" }
                        ));
                        captures.push(read_capture(
                            &path,
                            configuration,
                            count,
                            repeat,
                            enabled,
                            moving,
                            request.trim_frames,
                        )?);
                    }
                }
            }
        }
    }
    let deltas = deltas(&request, &captures)?;
    validate(&captures, &deltas)?;
    write_outputs(&directory, &request, &captures, &deltas)?;
    println!("Artifacts: {}", directory.display());
    Ok(())
}

#[cfg(test)]
#[path = "../tests/gpu/mod.rs"]
mod tests;
