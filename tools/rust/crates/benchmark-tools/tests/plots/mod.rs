use super::*;
use crate::results::{Comparison, Conditions, Identity, PairedMetric, Summary};
use serde_json::json;
use std::fs;

fn fixture() -> Value {
    serde_json::from_str(include_str!("kernel-benchmark-results.json")).unwrap()
}

fn assert_svg(path: &Path, labels: &[&str]) {
    let svg = fs::read_to_string(path).unwrap();
    assert!(svg.starts_with("<svg"));
    assert!(svg.contains("</svg>"));
    for label in labels {
        assert!(svg.contains(label), "Missing {label} in {}", path.display());
    }
}

pub fn fighters() -> Value {
    json!([{
        "fighter_stress":{"configured_cap":2000},
        "timing":{"mean_tick_microseconds":120.0,"median_tick_microseconds":100.0,"p95_tick_microseconds":150.0,"p99_tick_microseconds":180.0,"ticks_per_second":8333.0}
    }])
}

pub fn comparison() -> Comparison {
    let metrics = [
        ("mean_tick_microseconds", "us"),
        ("median_tick_microseconds", "us"),
        ("p95_tick_microseconds", "us"),
        ("p99_tick_microseconds", "us"),
        ("ticks_per_second", "ticks/s"),
    ]
    .into_iter()
    .map(|(metric, unit)| PairedMetric {
        identity: Identity {
            metric: metric.into(),
            unit: unit.into(),
            dimensions: Conditions::from([("case".into(), "fighters-2000".into())]),
        },
        baseline: Summary::from_samples([100.0, 120.0]).unwrap(),
        candidate: Summary::from_samples([90.0, 110.0]).unwrap(),
        delta: Summary::from_samples([-10.0, -10.0]).unwrap(),
        delta_percent: Some(Summary::from_samples([-10.0, -8.333]).unwrap()),
        pairs: Vec::new(),
    })
    .collect();
    Comparison {
        comparable: true,
        errors: Vec::new(),
        metrics,
    }
}

#[test]
fn kernel_fixture_renders_expected_chart_families() {
    let output = ioj_test_support::temp_dir("kernel svg");
    let paths = plot_kernel(&fixture(), output.path(), "scalar").unwrap();
    assert_eq!(paths.len(), 7);
    for path in &paths {
        assert_svg(path, &["Element count", "<polyline"]);
    }
    assert!(
        paths
            .iter()
            .any(|p| p.to_string_lossy().contains("alignment-penalty"))
    );
    assert!(
        paths
            .iter()
            .any(|p| p.to_string_lossy().contains("layout-speedup"))
    );
}

#[test]
fn missing_baseline_keeps_absolute_single_point_charts_and_omits_unmatched_pairs() {
    let output = ioj_test_support::temp_dir("sparse kernel svg");
    let data = json!({"benchmarks":[
        {"run_name":"dot/relaxed/flat/avx2/ordinary/aligned/32/real_time","real_time":2,"time_unit":"ns"},
        {"run_name":"dot/relaxed/chunked16/avx2/ordinary/unaligned/64/real_time","real_time":3,"time_unit":"ns"}
    ]});
    let paths = plot_kernel(&data, output.path(), "scalar").unwrap();
    assert_eq!(paths.len(), 4);
    for path in paths {
        assert!(!fs::read_to_string(path).unwrap().contains("Speedup"));
    }
}

#[test]
fn fallback_baseline_and_aos_layouts_render() {
    let output = ioj_test_support::temp_dir("aos kernel svg");
    let data = json!({"benchmarks":[
        {"run_name":"dot/relaxed/aos/autovec-avx2/ordinary/aligned/32/real_time","real_time":4,"time_unit":"ns"},
        {"run_name":"dot/relaxed/soa-flat/autovec-avx2/ordinary/aligned/32/real_time","real_time":2,"time_unit":"ns"}
    ]});
    let paths = plot_kernel(&data, output.path(), "scalar").unwrap();
    assert_eq!(paths.len(), 4);
    assert_svg(
        &output.path().join("dot-relaxed-aos-ordinary-aligned.svg"),
        &["Speedup over autovec-avx2"],
    );
    assert_svg(
        &output
            .path()
            .join("dot-relaxed-ordinary-aos-layout-speedup.svg"),
        &["Speedup over aos"],
    );
}

#[test]
fn native_charts_have_metric_units_and_direction_labels() {
    let output = ioj_test_support::temp_dir("native svg");
    let fighter_paths = plot_fighters(&fighters(), &output.path().join("fighter")).unwrap();
    assert_eq!(fighter_paths.len(), 2);
    assert_svg(
        &fighter_paths[0],
        &["lower is better", "p95", "p99", "Fighter cap"],
    );
    let paths = plot_comparison(&comparison(), &output.path().join("comparison")).unwrap();
    assert_eq!(paths.len(), 3);
    assert_svg(
        &paths[0],
        &["Baseline", "Candidate", "fighters-2000", "(us)"],
    );
    assert_svg(&paths[1], &["higher is better", "ticks/s"]);
    assert_svg(
        &paths[2],
        &["Median paired change", "not statistical significance"],
    );
}

#[test]
fn incomparable_results_produce_no_plots() {
    let output = ioj_test_support::temp_dir("incomparable svg");
    let destination = output.path().join("plots");
    let mut data = comparison();
    data.comparable = false;
    assert!(plot_comparison(&data, &destination).is_err());
    assert!(!destination.exists());
}

#[test]
fn automatic_failure_preserves_measurements_and_offline_failure_is_an_error() {
    let output = ioj_test_support::temp_dir("failed svg");
    let results = output.path().join("results.json");
    write_json(&results, &fighters()).unwrap();
    let blocked = output.path().join("plots");
    fs::write(&blocked, "occupied").unwrap();
    assert!(automatic(|| plot_fighters(&fighters(), &blocked)).is_empty());
    assert_eq!(read_json::<Value>(&results).unwrap(), fighters());
    let args = [
        "--kind",
        "fighter",
        "--input",
        results.to_str().unwrap(),
        "--output-dir",
        blocked.to_str().unwrap(),
    ]
    .map(str::to_owned);
    let args = std::iter::once("plot".to_owned())
        .chain(args)
        .collect::<Vec<_>>();
    assert!(crate::dispatch_benchmark_command(&args).is_err());
}

#[test]
fn offline_commands_regenerate_saved_results() {
    let output = ioj_test_support::temp_dir("offline svg");
    for (kind, data, count) in [
        ("kernel", fixture(), 7),
        ("fighter", fighters(), 2),
        (
            "comparison",
            json!({"schemaVersion":1,"comparison":comparison()}),
            3,
        ),
    ] {
        let input = output.path().join(format!("{kind}.json"));
        let destination = output.path().join(kind);
        write_json(&input, &data).unwrap();
        crate::dispatch_benchmark_command(
            &[
                "plot",
                "--kind",
                kind,
                "--input",
                input.to_str().unwrap(),
                "--output-dir",
                destination.to_str().unwrap(),
            ]
            .map(str::to_owned),
        )
        .unwrap();
        assert_eq!(fs::read_dir(destination).unwrap().count(), count);
    }
}
