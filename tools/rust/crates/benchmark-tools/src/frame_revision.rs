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

pub fn execute(root: &Path, args: &[String]) -> Result<()> {
    let parsed = Args::parse(
        args,
        &[
            "--iterations",
            "--warmup-iterations",
            "--baseline",
            "--output-dir",
            "--baseline-worktree",
        ],
        &[
            "--skip-build",
            "--prepare-only",
            "--validate-only",
            "--keep-baseline-worktree",
        ],
    )?;
    if parsed.flag("--prepare-only") && parsed.flag("--validate-only") {
        return Err("--prepare-only and --validate-only are mutually exclusive.".into());
    }
    let validate = parsed.flag("--validate-only");
    let iterations = parsed.integer("--iterations", 5, 1, 100)?;
    let warmups = parsed.integer("--warmup-iterations", 0, 0, 10)?;
    let (iterations, warmups) = if validate {
        (1, 0)
    } else {
        (iterations, warmups)
    };
    let supplied = parsed.value("--baseline-worktree", "");
    if parsed.flag("--skip-build") && supplied.is_empty() {
        return Err("--skip-build requires --baseline-worktree.".into());
    }
    let mut run = Run::new(
        root,
        "frame-memory-revision-ab",
        Path::new(parsed.value("--output-dir", ".local/benchmarks/frame-memory-revision-ab")),
        json!({"iterations":iterations,"warmupIterations":warmups,"seconds":20,"gameSpeed":100}),
        "",
        true,
    )?;
    run.manifest["purpose"] = json!(if validate {
        "validation"
    } else {
        "measurement"
    });
    run.publish()?;
    println!("Artifacts: {}", run.directory.display());
    if validate {
        println!("Validation only: one smoke A/B pair; no performance conclusions.");
    }
    let result = (|| {
        let revisions = Revisions::new(
            root,
            parsed.value("--baseline", "HEAD"),
            if supplied.is_empty() {
                None
            } else {
                Some(supplied)
            },
            parsed.flag("--keep-baseline-worktree"),
            &run.directory,
        )?;
        let operation = (|| {
            run.manifest["provenance"] = json!({"candidate":revisions.candidate,"baseline":revisions.baseline,"ownsBaseline":revisions.owned,
                "orchestrator":std::env::current_exe()?,"effectiveArguments":args});
            run.publish()?;
            if !parsed.flag("--skip-build") {
                let modules = [
                    "native/third_party/googletest",
                    "native/third_party/cpu_features",
                    "native/third_party/tracy",
                    "native/third_party/cli11",
                ];
                revision::initialize_submodules(root, None, Some(&modules))?;
                build(root)?;
                if revisions.owned {
                    revision::initialize_submodules(
                        &revisions.baseline.root,
                        Some(root),
                        Some(&modules),
                    )?;
                }
                build(&revisions.baseline.root)?;
            }
            revision::verify_source(&revisions.candidate)?;
            revision::verify_source(&revisions.baseline)?;
            if parsed.flag("--prepare-only") {
                return Ok(());
            }
            let sequence = revision::balanced(iterations, warmups);
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
            run.expect("records.json")?;
            run.status("measuring")?;
            let mut records = Vec::new();
            for item in sequence {
                let source = if item.side == "baseline" {
                    &revisions.baseline
                } else {
                    &revisions.candidate
                };
                let mut command = native::frame_args(&source.root, 20.0);
                command.push("--skip-build".into());
                let values = json_lines(&native::simulation(&source.root, &command)?)?;
                if values.len() != 1 {
                    return Err("NativeFrameMemoryLevel must produce one JSON result.".into());
                }
                let value = &values[0];
                let mean_tick_us = number(value, "/timing/mean_tick_microseconds")?;
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
            revision::verify_source(&revisions.candidate)?;
            revision::verify_source(&revisions.baseline)?;
            run.validate()?;
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
            run.expect("raw-results.csv")?;
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
                run.expect("paired-results.csv")?;
                println!(
                    "native: mean delta {:.3}%, median delta {:.3}%",
                    percentages.iter().sum::<f64>() / percentages.len() as f64,
                    Summary::across(percentages)?.median
                );
            }
            Ok(())
        })();
        revisions.finish(operation)
    })();
    run.finish(result)
}
fn build(root: &Path) -> Result<()> {
    visible(root, "cmake", &["--preset", "native-benchmark"])?;
    visible(
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
