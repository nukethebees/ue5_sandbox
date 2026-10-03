use crate::{
    native,
    results::{self, Capture, Conditions, Identity, Metric, Summary},
    revision::{self, Revisions, Run, Source},
    support::*,
};
use serde_json::{Value, json};
use std::{fs, path::Path};

const HELP: &str = "Usage: agent-task benchmark compare --baseline <ref> [options]
  --candidate <ref>         Compare a second commit (default: current working tree)
  --workload <name>         fighter-simulation (default) or native-simulation
  --level <relative-path>   Required for native-simulation; resolved in each revision
  --fighter-caps <caps>     Fighter workload populations (default: 2000,4000)
  --seconds <seconds>       Simulated duration per case (fighter: 10, native: 5)
  --warmup-seconds <n>      Fighter steady-state warmup (default: 5)
  --saturation-timeout-seconds <n>  Fighter saturation limit (default: 60)
  --game-speed <n>          Native simulation game speed (default: 1)
  --repetitions <n>         Complete pairs, alternating AB/BA (default: 2)
  --order <sequence>        Explicit A/B order, e.g. ABAB or AABB; replaces --repetitions
  --prepare-only           Build both sides and retain comparison worktrees
  --baseline-worktree <path>  Reuse a prepared baseline inside this workspace
  --candidate-worktree <path> Reuse a prepared candidate; requires --candidate
  --skip-build             Use prepared binaries; requires supplied commit worktrees
  --keep-worktrees         Retain newly created worktrees after measurement
  --output-dir <path>      Parent for unique runs (default: .local/benchmarks/compare)
Results: raw JSON per process, captures.json, comparison.json/.csv/.md, source.diff.
Use a shared jobs ticket for preparation and an exclusive ticket for measurement.";

struct NativeBenchmarkConfig {
    level: String,
    caps: Vec<u32>,
    seconds: f64,
    speed: u32,
    warmup: f64,
    timeout: f64,
}

fn parse_measurement_sequence(args: &Args) -> Result<Vec<revision::Repetition>> {
    let Some(order) = args.optional("--order") else {
        return Ok(revision::balanced_repetitions(
            args.integer("--repetitions", 2, 1, 100)?,
            0,
        ));
    };
    if args.flag("--repetitions") {
        return Err(
            "--order determines the run count and cannot be combined with --repetitions.".into(),
        );
    }
    let order = order.to_ascii_uppercase();
    let count_a = order.bytes().filter(|&side| side == b'A').count();
    let count_b = order.bytes().filter(|&side| side == b'B').count();
    if count_a == 0 || count_a != count_b || count_a > 100 || count_a + count_b != order.len() {
        return Err(
            "--order must contain only A and B, with equal counts of 1 to 100 per side.".into(),
        );
    }
    let mut repetitions = [0, 0];
    Ok(order
        .bytes()
        .enumerate()
        .map(|(index, side)| {
            let side_index = usize::from(side == b'B');
            repetitions[side_index] += 1;
            revision::Repetition {
                sequence: index as u32 + 1,
                repetition: repetitions[side_index],
                side: if side == b'A' {
                    "baseline"
                } else {
                    "candidate"
                }
                .into(),
                warmup: false,
            }
        })
        .collect())
}

impl NativeBenchmarkConfig {
    fn parse_native_benchmark_options(args: &Args) -> Result<Self> {
        let fighter = args.choice(
            "--workload",
            "fighter-simulation",
            &["fighter-simulation", "native-simulation"],
        )? == "fighter-simulation";
        if fighter && (args.flag("--level") || args.flag("--game-speed")) {
            return Err("--level and --game-speed apply only to native-simulation.".into());
        }
        if !fighter
            && [
                "--fighter-caps",
                "--warmup-seconds",
                "--saturation-timeout-seconds",
            ]
            .iter()
            .any(|arg| args.flag(arg))
        {
            return Err("Fighter controls apply only to fighter-simulation.".into());
        }
        let level = if fighter {
            "LevelScripts/FighterSchedulingBenchmark.scm"
        } else {
            args.required("--level")?
        };
        if Path::new(level).is_absolute()
            || Path::new(level).components().any(|component| {
                !matches!(
                    component,
                    std::path::Component::Normal(_) | std::path::Component::CurDir
                )
            })
        {
            return Err(
                "--level must be a path within each revision, relative to its root.".into(),
            );
        }
        Ok(Self {
            level: level.into(),
            caps: if fighter {
                native::parse_fighter_caps(args.value("--fighter-caps", "2000,4000"))?
            } else {
                vec![]
            },
            seconds: args.float("--seconds", if fighter { 10.0 } else { 5.0 }, 0.001, 180.0)?,
            speed: args.integer("--game-speed", 1, 1, u32::MAX)?,
            warmup: args.float("--warmup-seconds", 5.0, 0.0, 180.0)?,
            timeout: args.float("--saturation-timeout-seconds", 60.0, 0.1, 180.0)?,
        })
    }

    fn build_simulation_arguments(&self, source: &Source) -> Vec<String> {
        let mut args = vec![
            "--level".into(),
            source.root.join(&self.level).to_string_lossy().into_owned(),
            "--seconds".into(),
            self.seconds.to_string(),
            "--game-speed".into(),
            self.speed.to_string(),
            "--skip-build".into(),
        ];
        if !self.caps.is_empty() {
            args.extend([
                "--fighter-stress-caps".into(),
                self.caps
                    .iter()
                    .map(u32::to_string)
                    .collect::<Vec<_>>()
                    .join(","),
                "--warmup-seconds".into(),
                self.warmup.to_string(),
                "--saturation-timeout-seconds".into(),
                self.timeout.to_string(),
            ]);
        }
        args
    }

    fn parse_native_capture(
        &self,
        values: &[Value],
        item: revision::Repetition,
        directory: &Path,
    ) -> Result<Capture> {
        let count = self.caps.len().max(1);
        if values.len() != count {
            return Err(format!(
                "Expected {count} benchmark results, found {}.",
                values.len()
            )
            .into());
        }

        let mut metrics = Vec::new();
        let mut conditions = Conditions::new();
        for (index, value) in values.iter().enumerate() {
            self.validate_native_result(value, index)?;

            let case = self
                .caps
                .get(index)
                .map_or_else(|| self.level.clone(), |cap| format!("fighters-{cap}"));
            conditions.extend(read_case_conditions(value, &case)?);
            metrics.extend(read_case_metrics(value, &case)?);
        }

        Ok(Capture {
            run_id: format!("{}-{}", item.sequence, item.side),
            directory: directory.to_string_lossy().into_owned(),
            repetition: item,
            conditions,
            metrics,
            schema_version: 1,
        })
    }

    fn validate_native_result(&self, value: &Value, index: usize) -> Result<()> {
        let ticks = (self.seconds * 60.0).ceil() as u64;

        if value["schema_version"] != 2
            || value["workload"]["requested_ticks"].as_u64() != Some(ticks)
            || value["workload"]["completed_ticks"].as_u64() != Some(ticks)
            || value["workload"]["measured_ticks"].as_u64() != Some(ticks)
            || value["workload"]["game_speed"].as_u64() != Some(self.speed.into())
            || value["memory"]["frame_overflow_count"] != 0
        {
            return Err("Unsupported or incomplete native benchmark result.".into());
        }

        if let Some(&cap) = self.caps.get(index) {
            native::validate_fighter_result(value, cap, ticks)?;
        } else if value["fighter_stress"]["enabled"] != false {
            return Err("Unexpected fighter stress workload.".into());
        }

        Ok(())
    }
}

fn read_case_conditions(value: &Value, case: &str) -> Result<Conditions> {
    let mut conditions = Conditions::new();

    // Exclude timings and allocation sizes; preserve workload and execution conditions.
    for pointer in [
        "/schema_version",
        "/level/id",
        "/environment",
        "/telemetry/enabled",
        "/workload/requested_ticks",
        "/workload/completed_ticks",
        "/workload/measured_ticks",
        "/workload/requested_seconds",
        "/workload/game_speed",
        "/workload/tick_rate_hz",
        "/workload/warmup_ticks",
        "/workload/advance_calls",
        "/final_state/initial_capital_ships",
        "/final_state/initial_turrets",
    ] {
        let field = value
            .pointer(pointer)
            .filter(|field| !field.is_null())
            .ok_or_else(|| format!("Missing comparability field: {pointer}"))?;
        conditions.insert(format!("{case}{pointer}"), field.to_string());
    }

    Ok(conditions)
}

fn read_case_metrics(value: &Value, case: &str) -> Result<Vec<Metric>> {
    let mut metrics = Vec::new();

    for (metric, unit) in [
        ("mean_tick_microseconds", "us"),
        ("median_tick_microseconds", "us"),
        ("p95_tick_microseconds", "us"),
        ("p99_tick_microseconds", "us"),
        ("ticks_per_second", "ticks/s"),
        ("realtime_factor", "ratio"),
    ] {
        let sample = read_finite_number(value, &format!("/timing/{metric}"))?;
        if sample <= 0.0 {
            return Err(format!("Invalid timing metric: {metric}").into());
        }

        metrics.push(Metric {
            identity: Identity {
                metric: metric.into(),
                unit: unit.into(),
                dimensions: Conditions::from([("case".into(), case.to_owned())]),
            },
            summary: Summary::from_samples([sample])?,
        });
    }

    Ok(metrics)
}

pub fn run_native_comparison(arguments: &[String]) -> Result<()> {
    if arguments.iter().any(|arg| arg == "--help" || arg == "-h") {
        println!("{HELP}");
        return Ok(());
    }
    let args = Args::parse_command_line(
        arguments,
        &[
            "--baseline",
            "--candidate",
            "--workload",
            "--level",
            "--fighter-caps",
            "--seconds",
            "--warmup-seconds",
            "--saturation-timeout-seconds",
            "--game-speed",
            "--repetitions",
            "--order",
            "--baseline-worktree",
            "--candidate-worktree",
            "--output-dir",
        ],
        &["--prepare-only", "--skip-build", "--keep-worktrees"],
    )?;
    let baseline = args.required("--baseline")?;
    let config = NativeBenchmarkConfig::parse_native_benchmark_options(&args)?;
    let sequence = parse_measurement_sequence(&args)?;
    if args.flag("--candidate-worktree") && !args.flag("--candidate") {
        return Err("--candidate-worktree requires --candidate.".into());
    }
    if args.flag("--prepare-only") && args.flag("--skip-build") {
        return Err("--prepare-only and --skip-build are mutually exclusive.".into());
    }
    if args.flag("--skip-build")
        && (!args.flag("--baseline-worktree")
            || (args.flag("--candidate") && !args.flag("--candidate-worktree")))
    {
        return Err("--skip-build requires a supplied worktree for each explicit commit.".into());
    }
    let root = find_repository_root(&std::env::current_dir()?)?;
    for option in ["--baseline-worktree", "--candidate-worktree"] {
        if let Some(path) = args.optional(option) {
            revision::validate_owned_path(&resolve_absolute_path(&root, path)?, &root)?;
        }
    }
    let mut run = Run::new(
        &root,
        "compare",
        Path::new(args.value("--output-dir", ".local/benchmarks/compare")),
        json!({"arguments":arguments,"sequence":sequence}),
        "",
        true,
    )?;
    println!("Artifacts: {}", run.directory.display());
    if args.flag("--prepare-only") {
        run.manifest["purpose"] = json!("preparation");
        run.write_manifest()?;
    }

    // Capture failures at each ownership boundary so every acquired worktree is finalized.
    let result = (|| {
        let mut baseline_input = Revisions::new(
            &root,
            baseline,
            args.optional("--baseline-worktree"),
            args.flag("--keep-worktrees"),
            &run.directory,
        )?;
        let result = (|| {
            let mut candidate_input = if let Some(candidate) = args.optional("--candidate") {
                Some(Revisions::new(
                    &root,
                    candidate,
                    args.optional("--candidate-worktree"),
                    args.flag("--keep-worktrees"),
                    &run.directory,
                )?)
            } else {
                None
            };
            let candidate = candidate_input
                .as_ref()
                .map_or(&baseline_input.candidate, |input| &input.baseline);
            let baseline = &baseline_input.baseline;
            let operation = (|| {
                record_comparison_inputs(&mut run, &config, baseline, candidate)?;

                for source in [baseline, candidate] {
                    if !args.flag("--skip-build") {
                        prepare_native_benchmark(&root, &source.root)?;
                    }
                    revision::verify_source_unchanged(source)?;
                }

                write_source_diffs(&run, baseline, candidate)?;

                if args.flag("--prepare-only") {
                    return write_preparation_result(
                        &run,
                        baseline,
                        candidate,
                        arguments,
                        args.flag("--candidate"),
                    );
                }

                measure_and_report_comparison(&mut run, &config, &sequence, baseline, candidate)
            })();

            if operation.is_ok() && args.flag("--prepare-only") {
                baseline_input.keep = true;
                if let Some(input) = &mut candidate_input {
                    input.keep = true;
                }
            }

            match candidate_input {
                Some(input) => input.cleanup_worktrees(operation),
                None => operation,
            }
        })();
        baseline_input.cleanup_worktrees(result)
    })();
    run.finish_run(result)
}

fn record_comparison_inputs(
    run: &mut Run,
    config: &NativeBenchmarkConfig,
    baseline: &Source,
    candidate: &Source,
) -> Result<()> {
    run.manifest["provenance"] =
        json!({"baseline":baseline,"candidate":candidate,"orchestrator":std::env::current_exe()?});
    run.write_manifest()?;

    let level_a = baseline.root.join(&config.level);
    let level_b = candidate.root.join(&config.level);
    revision::validate_owned_path(&level_a, &baseline.root)?;
    revision::validate_owned_path(&level_b, &candidate.root)?;
    let level = fs::read(&level_a)?;

    if level != fs::read(&level_b)? {
        return Err("Level scripts differ between revisions; choose an unchanged workload.".into());
    }

    run.manifest["configuration"]["levelSha256"] = json!(revision::sha256_hex(&level));
    Ok(())
}

fn write_source_diffs(run: &Run, baseline: &Source, candidate: &Source) -> Result<()> {
    let diff = revision::run_git_capture(
        &candidate.root,
        &[
            "diff",
            "--no-ext-diff",
            "--no-textconv",
            "--binary",
            &baseline.commit,
            &candidate.commit,
            "--",
        ],
    )?;
    write_text(&run.path("source.diff"), diff)?;

    if candidate.dirty {
        write_text(
            &run.path("candidate-working-tree.diff"),
            revision::run_git_capture(
                &candidate.root,
                &[
                    "diff",
                    "HEAD",
                    "--binary",
                    "--no-ext-diff",
                    "--no-textconv",
                    "--",
                ],
            )?,
        )?;
    }

    Ok(())
}

fn write_preparation_result(
    run: &Run,
    baseline: &Source,
    candidate: &Source,
    arguments: &[String],
    explicit_candidate: bool,
) -> Result<()> {
    write_json(
        &run.path("preparation.json"),
        &json!({"baseline":baseline,"candidate":candidate,"arguments":arguments}),
    )?;
    println!(
        "Prepared baseline: {}\nPrepared candidate: {}",
        baseline.root.display(),
        candidate.root.display()
    );
    println!(
        "Repeat the same command with --skip-build --baseline-worktree \"{}\"{} (omit --prepare-only).",
        baseline.root.display(),
        if explicit_candidate {
            format!(" --candidate-worktree \"{}\"", candidate.root.display())
        } else {
            String::new()
        }
    );
    Ok(())
}

fn measure_and_report_comparison(
    run: &mut Run,
    config: &NativeBenchmarkConfig,
    sequence: &[revision::Repetition],
    baseline: &Source,
    candidate: &Source,
) -> Result<()> {
    revision::verify_source_unchanged(baseline)?;
    revision::verify_source_unchanged(candidate)?;

    write_json(&run.path("sequence.json"), &sequence)?;
    run.expect_artifact("sequence.json")?;
    run.set_status("measuring")?;
    let captures = collect_native_captures(run, config, sequence, baseline, candidate)?;

    revision::verify_source_unchanged(baseline)?;
    revision::verify_source_unchanged(candidate)?;

    let comparable = write_native_comparison_reports(run, &captures)?;
    for name in [
        "captures.json",
        "comparison.json",
        "comparison.csv",
        "comparison.md",
    ] {
        run.expect_artifact(name)?;
    }
    run.validate_artifacts()?;

    if !comparable {
        return Err("Benchmark conditions differ; see comparison.md. No deltas published.".into());
    }

    Ok(())
}

fn collect_native_captures(
    run: &Run,
    config: &NativeBenchmarkConfig,
    sequence: &[revision::Repetition],
    baseline: &Source,
    candidate: &Source,
) -> Result<Vec<Capture>> {
    let mut captures = Vec::new();

    for item in sequence.iter().cloned() {
        let source = if item.side == "baseline" {
            baseline
        } else {
            candidate
        };
        let directory = run.path(&format!("runs/{}-{}", item.sequence, item.side));
        let output = native::run_simulation_benchmark_with_logs(
            &source.root,
            &config.build_simulation_arguments(source),
            &directory,
        )?;
        let values = parse_json_lines(&output)?;
        write_json(&directory.join("results.json"), &values)?;
        captures.push(config.parse_native_capture(&values, item, &directory)?);
        write_json(&run.path("captures.json"), &captures)?;
    }

    Ok(captures)
}

fn prepare_native_benchmark(root: &Path, target: &Path) -> Result<()> {
    // Derive dependencies from each commit so historical revisions need only their pinned modules.
    let tree = revision::run_git_capture(
        target,
        &["ls-tree", "-r", "-z", "HEAD", "--", "native/third_party"],
    )?;
    let modules: Vec<_> = tree
        .split('\0')
        .filter(|entry| entry.starts_with("160000 "))
        .filter_map(|entry| entry.split_once('\t').map(|(_, path)| path))
        .collect();
    if !modules.is_empty() {
        revision::initialize_submodules(
            target,
            if target == root { None } else { Some(root) },
            Some(&modules),
        )?;
    }
    run_process_inherited(target, "cmake", &["--preset", "native-benchmark"])?;
    run_process_inherited(
        target,
        "cmake",
        &[
            "--build",
            "--preset",
            "native-simulation-benchmark",
            "--target",
            "native-simulation-benchmark",
        ],
    )
}

fn write_native_comparison_reports(run: &Run, captures: &[Capture]) -> Result<bool> {
    let comparison = results::compare_captures(captures)?;
    write_json(
        &run.path("comparison.json"),
        &json!({"schemaVersion":1,"provenance":run.manifest["provenance"],"comparison":comparison}),
    )?;
    let mut text = format!(
        "# Native revision comparison\n\nBaseline: `{}`. Candidate: `{}` ({}).\n\nDeltas are candidate minus baseline, paired by repetition. Timing reductions are improvements; throughput increases are improvements. Values summarize complete runs; tick samples are not independent repetitions.\n\n",
        run.manifest["provenance"]["baseline"]["commit"]
            .as_str()
            .unwrap_or(""),
        run.manifest["provenance"]["candidate"]["commit"]
            .as_str()
            .unwrap_or(""),
        if run.manifest["provenance"]["candidate"]["dirty"] == true {
            "dirty working tree"
        } else {
            "clean"
        }
    );
    let order: String = captures
        .iter()
        .map(|capture| {
            if capture.repetition.side == "baseline" {
                'A'
            } else {
                'B'
            }
        })
        .collect();
    text.push_str(&format!(
        "Order: `{order}` (A = baseline, B = candidate). The nth A and nth B form a pair.\n\n"
    ));
    let mut rows = vec![
        [
            "case",
            "metric",
            "unit",
            "pairs",
            "baseline_median",
            "candidate_median",
            "paired_delta_median",
            "paired_delta_percent_median",
        ]
        .map(str::to_owned)
        .to_vec(),
    ];
    text.push_str("| Case | Metric | Unit | Pairs | Baseline | Candidate | Delta | Delta % |\n|---|---|---|---:|---:|---:|---:|---:|\n");
    for metric in &comparison.metrics {
        let case = &metric.identity.dimensions["case"];
        let percent = metric
            .delta_percent
            .as_ref()
            .map(|value| format!("{:.3}", value.median))
            .unwrap_or_default();
        text.push_str(&format!(
            "| {} | {} | {} | {} | {:.3} | {:.3} | {:.3} | {} |\n",
            case.replace('|', "\\|"),
            metric.identity.metric,
            metric.identity.unit,
            metric.delta.samples,
            metric.baseline.median,
            metric.candidate.median,
            metric.delta.median,
            percent
        ));
        rows.push(vec![
            case.clone(),
            metric.identity.metric.clone(),
            metric.identity.unit.clone(),
            metric.delta.samples.to_string(),
            metric.baseline.median.to_string(),
            metric.candidate.median.to_string(),
            metric.delta.median.to_string(),
            metric
                .delta_percent
                .as_ref()
                .map(|value| value.median.to_string())
                .unwrap_or_default(),
        ]);
    }
    for error in &comparison.errors {
        text.push_str(&format!("\nIncomparable: {error}\n"));
    }
    write_csv(&run.path("comparison.csv"), &rows)?;
    write_text(&run.path("comparison.md"), &text)?;
    println!("{text}\nReport: {}", run.path("comparison.md").display());
    Ok(comparison.comparable)
}
