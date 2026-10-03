use super::*;

fn captures() -> Vec<Capture> {
    ["baseline", "candidate"].into_iter().enumerate().map(|(index, side)| Capture {
        run_id: side.into(), directory: side.into(),
        repetition: revision::Repetition { sequence: index as u32 + 1, repetition: 1, side: side.into(), warmup: false },
        conditions: Conditions::from([("workload".into(), "fixture".into())]),
        metrics: read_case_metrics(&json!({"timing":{
            "mean_tick_microseconds":120,"median_tick_microseconds":100,"p95_tick_microseconds":150,
            "p99_tick_microseconds":180,"ticks_per_second":8333,"realtime_factor":138
        }}), "fighters-2000").unwrap(),
        schema_version: 2,
    }).collect()
}

fn run(root: &Path) -> Run {
    Run::new(root, "native-comparison", root, json!({}), "fixture", false).unwrap()
}

#[test]
fn comparison_reports_link_only_successfully_written_charts() {
    let output = ioj_test_support::temp_dir("comparison report plots");
    let mut run = run(output.path());
    assert!(write_native_comparison_reports(&mut run, &captures()).unwrap());
    let markdown = fs::read_to_string(run.path("comparison.md")).unwrap();
    for name in ["timings", "throughput", "deltas"] {
        let file = format!("plots/{name}.svg");
        assert!(markdown.contains(&format!("]({file})")));
        assert!(
            run.manifest["expectedArtifacts"]
                .as_array()
                .unwrap()
                .contains(&json!(file))
        );
    }
    run.validate_artifacts().unwrap();
}

#[test]
fn incomparable_reports_and_plot_failures_have_no_broken_links() {
    let output = ioj_test_support::temp_dir("failed comparison plots");
    let mut incomparable = run(output.path());
    let mut data = captures();
    data[1]
        .conditions
        .insert("workload".into(), "different".into());
    assert!(!write_native_comparison_reports(&mut incomparable, &data).unwrap());
    assert!(!incomparable.path("plots").exists());

    let mut blocked = run(output.path());
    fs::write(blocked.path("plots"), "occupied").unwrap();
    assert!(write_native_comparison_reports(&mut blocked, &captures()).unwrap());
    assert!(blocked.path("comparison.json").is_file());
    assert!(blocked.path("comparison.csv").is_file());
    assert!(
        !fs::read_to_string(blocked.path("comparison.md"))
            .unwrap()
            .contains("](plots/")
    );
    blocked.validate_artifacts().unwrap();
}
