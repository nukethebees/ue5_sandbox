use super::*;
use crate::results::Comparison;

pub fn plot_fighters(document: &Value, output: &Path) -> Result<Vec<PathBuf>> {
    let values = document
        .as_array()
        .filter(|values| !values.is_empty())
        .ok_or("Expected a nonempty fighter results array.")?;
    let mut latency = Vec::new();
    let mut throughput = Vec::new();
    for (metric, label) in [
        ("mean_tick_microseconds", "Mean"),
        ("median_tick_microseconds", "Median"),
        ("p95_tick_microseconds", "p95"),
        ("p99_tick_microseconds", "p99"),
        ("ticks_per_second", "Ticks/s"),
    ] {
        let mut points = Vec::new();
        for value in values {
            let cap = value
                .pointer("/fighter_stress/configured_cap")
                .and_then(Value::as_u64)
                .filter(|cap| *cap > 0)
                .ok_or("Missing positive fighter cap.")?;
            let y = read_finite_number(value, &format!("/timing/{metric}"))?;
            if y <= 0.0 {
                return Err("Fighter timings must be positive.".into());
            }
            points.push(Point {
                x: cap as f64,
                y,
                deviation: 0.0,
            });
        }
        points.sort_by(|a, b| a.x.total_cmp(&b.x));
        if points.windows(2).any(|pair| pair[0].x == pair[1].x) {
            return Err("Duplicate fighter cap.".into());
        }
        let series = Series {
            label: label.into(),
            points,
        };
        if metric == "ticks_per_second" {
            throughput.push(series);
        } else {
            latency.push(series);
        }
    }
    let timings = output.join("tick-latency.svg");
    let rate = output.join("throughput.svg");
    line_plot(
        &timings,
        "Fighter tick latency (lower is better)",
        "Fighter cap",
        false,
        &[Panel {
            label: "Tick latency (us)".into(),
            series: latency,
            reference: None,
        }],
    )?;
    line_plot(
        &rate,
        "Fighter throughput (higher is better)",
        "Fighter cap",
        false,
        &[Panel {
            label: "Throughput (ticks/s)".into(),
            series: throughput,
            reference: None,
        }],
    )?;
    Ok(vec![timings, rate])
}

pub fn plot_comparison(comparison: &Comparison, output: &Path) -> Result<Vec<PathBuf>> {
    if !comparison.comparable || !comparison.errors.is_empty() {
        return Err("Cannot plot incomparable benchmark results.".into());
    }
    let mut timings = Vec::new();
    let mut throughput = Vec::new();
    let mut deltas = Vec::new();
    for metric in &comparison.metrics {
        let label = match (
            metric.identity.metric.as_str(),
            metric.identity.unit.as_str(),
        ) {
            ("mean_tick_microseconds", "us") => "Mean tick",
            ("median_tick_microseconds", "us") => "Median tick",
            ("p95_tick_microseconds", "us") => "p95 tick",
            ("p99_tick_microseconds", "us") => "p99 tick",
            ("ticks_per_second", "ticks/s") => "Ticks/s",
            ("realtime_factor", "ratio") => continue,
            _ => return Err("Unsupported native comparison metric or unit.".into()),
        };
        metric.baseline.validate_summary()?;
        metric.candidate.validate_summary()?;
        let case = metric
            .identity
            .dimensions
            .get("case")
            .ok_or("Missing comparison case.")?;
        let label = format!("{case}: {label}");
        let row = BarRow {
            label: label.clone(),
            values: vec![metric.baseline.median, metric.candidate.median],
        };
        if row.values.iter().any(|value| *value <= 0.0) {
            return Err("Comparison timings must be positive.".into());
        }
        if metric.identity.unit == "us" {
            timings.push(row);
        } else {
            throughput.push(row);
        }
        if let Some(delta) = &metric.delta_percent {
            delta.validate_summary()?;
            deltas.push(BarRow {
                label,
                values: vec![delta.median],
            });
        }
    }
    if timings.is_empty() || throughput.is_empty() {
        return Err("Native comparison has no timing or throughput metrics.".into());
    }
    let timing_path = output.join("timings.svg");
    let throughput_path = output.join("throughput.svg");
    bar_plot(
        &timing_path,
        "Tick latency: median of complete runs (lower is better)",
        "Tick latency (us)",
        &["Baseline", "Candidate"],
        &timings,
    )?;
    bar_plot(
        &throughput_path,
        "Throughput: median of complete runs (higher is better)",
        "Throughput (ticks/s)",
        &["Baseline", "Candidate"],
        &throughput,
    )?;
    let mut written = vec![timing_path, throughput_path];
    if !deltas.is_empty() {
        let path = output.join("deltas.svg");
        bar_plot(
            &path,
            "Paired deltas: negative timing / positive throughput is better",
            "Median paired change (%) — not statistical significance",
            &["Candidate vs baseline"],
            &deltas,
        )?;
        written.push(path);
    }
    Ok(written)
}
