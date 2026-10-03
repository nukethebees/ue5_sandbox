use crate::cli::FrameComparisonOptions;
use crate::{
    native,
    results::Summary,
    revision::{self, Revisions, Run},
    support::*,
};
use serde::{Deserialize, Serialize};
use serde_json::json;
use std::path::Path;

#[derive(Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
struct Record {
    pair: u32,
    sequence: u32,
    state: String,
    commit: String,
    mean_tick_us: f64,
    peak_claimed_bytes: u64,
    peak_payload_bytes: u64,
    total_padding_bytes: u64,
    total_root_claims: u64,
}

pub fn run_frame_memory_comparison(
    root: &Path,
    parsed: &FrameComparisonOptions,
    args: &[String],
) -> Result<()> {
    let validate = parsed.validate_only;
    let iterations = parsed.iterations;
    let warmups = parsed.warmup_iterations;
    let (iterations, warmups) = if validate {
        (1, 0)
    } else {
        (iterations, warmups)
    };
    let supplied = parsed.baseline_worktree.as_deref();
    let mut run = Run::new(
        root,
        "frame-memory-revision-ab",
        &parsed.output_dir,
        json!({"iterations":iterations,"warmupIterations":warmups,"seconds":20,"gameSpeed":100}),
        "",
        true,
    )?;
    run.manifest["purpose"] = json!(if validate {
        "validation"
    } else {
        "measurement"
    });
    run.write_manifest()?;
    println!("Artifacts: {}", run.directory.display());
    if validate {
        println!("Validation only: one smoke A/B pair; no performance conclusions.");
    }
    let result = (|| {
        let revisions = Revisions::new(
            root,
            &parsed.baseline,
            supplied,
            parsed.keep_baseline_worktree,
            &run.directory,
        )?;
        let operation = (|| {
            run.manifest["provenance"] = json!({"candidate":revisions.candidate,"baseline":revisions.baseline,"ownsBaseline":revisions.owned,
                "orchestrator":std::env::current_exe()?,"effectiveArguments":args});
            run.write_manifest()?;
            if !parsed.skip_build {
                let modules = [
                    "native/third_party/googletest",
                    "native/third_party/cpu_features",
                    "native/third_party/tracy",
                    "native/third_party/cli11",
                ];
                revision::initialize_submodules(root, None, Some(&modules))?;
                build_frame_memory_benchmark(root)?;
                if revisions.owned {
                    revision::initialize_submodules(
                        &revisions.baseline.root,
                        Some(root),
                        Some(&modules),
                    )?;
                }
                build_frame_memory_benchmark(&revisions.baseline.root)?;
            }
            revision::verify_source_unchanged(&revisions.candidate)?;
            revision::verify_source_unchanged(&revisions.baseline)?;
            if parsed.prepare_only {
                return Ok(());
            }
            let sequence = revision::balanced_repetitions(iterations, warmups);
            write_json(&run.path("sequence.json"), &sequence)?;
            let plan = crate::ismc::Plan {
                output: run.directory.clone(),
                candidate: revisions.candidate.clone(),
                baseline: revisions.baseline.clone(),
                sequence: sequence.clone(),
                ismc: None,
                validation_only: validate,
            };
            write_json(&run.path("measurement-plan.json"), &plan)?;
            for name in ["sequence.json", "measurement-plan.json"] {
                run.manifest["artifacts"][name] = json!(run.path(name));
            }
            run.expect_artifact("records.json")?;
            run.set_status("measuring")?;
            let mut records = Vec::new();
            for item in sequence {
                let source = if item.side == "baseline" {
                    &revisions.baseline
                } else {
                    &revisions.candidate
                };
                let mut command = native::frame_memory_options(&source.root, 20.0);
                command.skip_build = true;
                let values =
                    parse_json_lines(&native::run_simulation_benchmark(&source.root, &command)?)?;
                if values.len() != 1 {
                    return Err("NativeFrameMemoryLevel must produce one JSON result.".into());
                }
                let value = &values[0];
                let mean_tick_us = read_finite_number(value, "/timing/mean_tick_microseconds")?;
                if mean_tick_us <= 0.0 {
                    return Err(
                        "NativeFrameMemoryLevel result contained an invalid mean tick time.".into(),
                    );
                }
                let memory = |key: &str| -> Result<u64> {
                    value["memory"][key]
                        .as_u64()
                        .ok_or_else(|| format!("Missing memory field: {key}").into())
                };
                let record = Record {
                    pair: item.repetition,
                    sequence: item.sequence,
                    state: item.side,
                    commit: source.commit.clone(),
                    mean_tick_us,
                    peak_claimed_bytes: memory("frame_peak_claimed_bytes")?,
                    peak_payload_bytes: memory("frame_peak_payload_bytes")?,
                    total_padding_bytes: memory("frame_total_padding_bytes")?,
                    total_root_claims: memory("frame_total_root_claims")?,
                };
                if !item.warmup {
                    records.push(record);
                }
                write_json(&run.path("records.json"), &records)?;
            }
            revision::verify_source_unchanged(&revisions.candidate)?;
            revision::verify_source_unchanged(&revisions.baseline)?;
            run.validate_artifacts()?;
            let mut rows = vec![
                [
                    "pair",
                    "sequence",
                    "state",
                    "commit",
                    "variant",
                    "mean_tick_us",
                    "peak_claimed_bytes",
                    "peak_payload_bytes",
                    "total_padding_bytes",
                    "total_root_claims",
                ]
                .map(str::to_owned)
                .to_vec(),
            ];
            for record in &records {
                rows.push(vec![
                    record.pair.to_string(),
                    record.sequence.to_string(),
                    record.state.clone(),
                    record.commit.clone(),
                    "native".into(),
                    record.mean_tick_us.to_string(),
                    record.peak_claimed_bytes.to_string(),
                    record.peak_payload_bytes.to_string(),
                    record.total_padding_bytes.to_string(),
                    record.total_root_claims.to_string(),
                ]);
            }
            write_csv(&run.path("raw-results.csv"), &rows)?;
            run.expect_artifact("raw-results.csv")?;
            if !validate {
                let mut rows = vec![
                    [
                        "pair",
                        "variant",
                        "baseline_tick_us",
                        "candidate_tick_us",
                        "delta_tick_us",
                        "delta_percent",
                    ]
                    .map(str::to_owned)
                    .to_vec(),
                ];
                let mut percentages = Vec::new();
                for pair in 1..=iterations {
                    let a = records
                        .iter()
                        .find(|r| r.pair == pair && r.state == "baseline")
                        .ok_or("Missing baseline record.")?
                        .mean_tick_us;
                    let b = records
                        .iter()
                        .find(|r| r.pair == pair && r.state == "candidate")
                        .ok_or("Missing candidate record.")?
                        .mean_tick_us;
                    let percent = 100.0 * (b - a) / a;
                    if !percent.is_finite() {
                        return Err("Frame-memory delta overflowed.".into());
                    }
                    percentages.push(percent);
                    rows.push(vec![
                        pair.to_string(),
                        "native".into(),
                        a.to_string(),
                        b.to_string(),
                        (b - a).to_string(),
                        percent.to_string(),
                    ]);
                }
                write_csv(&run.path("paired-results.csv"), &rows)?;
                run.expect_artifact("paired-results.csv")?;
                println!(
                    "native: mean delta {:.3}%, median delta {:.3}%",
                    percentages.iter().sum::<f64>() / percentages.len() as f64,
                    Summary::from_samples(percentages)?.median
                );
            }
            Ok(())
        })();
        revisions.cleanup_worktrees(operation)
    })();
    run.finish_run(result)
}
fn build_frame_memory_benchmark(root: &Path) -> Result<()> {
    run_process_inherited(root, "cmake", &["--preset", "native-benchmark"])?;
    run_process_inherited(
        root,
        "cmake",
        &[
            "--build",
            "--preset",
            "frame-memory-level-benchmark",
            "--target",
            "native-simulation-benchmark",
        ],
    )
}
