use crate::{
    ismc::{Plan, Request},
    revision::Repetition,
    support::*,
};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::{
    collections::{BTreeMap, BTreeSet},
    fs,
    path::Path,
};

pub type Conditions = BTreeMap<String, String>;
pub const CONDITIONS: &[&str] = &[
    "result_schema",
    "mode",
    "visibility",
    "bounds",
    "custom_data",
    "rhi",
    "instances",
    "update_percent",
    "churn",
    "min_instances",
    "half_cycle_updates",
    "replacement_percent",
    "warmup_updates",
    "warmup_seconds",
    "measurement_seconds",
    "shadows",
    "trace",
    "requested_width",
    "requested_height",
    "observed_width",
    "observed_height",
    "grid_spacing",
    "grid_gap",
    "movement_amplitude",
    "movement_frequency",
    "rotation_speed",
    "frame_limits_disabled",
    "r.Editor.Viewport.OverridePIEScreenPercentage",
    "r.ScreenPercentage",
    "r.DynamicRes.OperationMode",
    "r.VSync",
    "r.VSyncEditor",
    "t.MaxFPS",
];

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
pub struct Identity {
    pub metric: String,
    pub unit: String,
    pub dimensions: Conditions,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Summary {
    pub samples: usize,
    pub min: f64,
    pub median: f64,
    pub p95: f64,
    pub max: f64,
}
impl Summary {
    pub fn validate_summary(&self) -> Result<()> {
        if self.samples == 0
            || ![self.min, self.median, self.p95, self.max]
                .iter()
                .all(|x| x.is_finite())
            || self.min > self.median
            || self.median > self.p95
            || self.p95 > self.max
        {
            return Err(
                "Metric summary has no samples, non-finite values, or inconsistent quantiles."
                    .into(),
            );
        }
        Ok(())
    }
    pub fn from_samples(values: impl IntoIterator<Item = f64>) -> Result<Self> {
        let mut values: Vec<_> = values.into_iter().collect();
        if values.is_empty() || !values.iter().all(|v| v.is_finite()) {
            return Err("Run summaries must contain finite values.".into());
        }
        values.sort_by(f64::total_cmp);
        let n = values.len();
        let median = if n % 2 == 0 {
            values[n / 2 - 1] / 2.0 + values[n / 2] / 2.0
        } else {
            values[n / 2]
        };
        Ok(Self {
            samples: n,
            min: values[0],
            median,
            p95: values[(0.95 * n as f64).ceil() as usize - 1],
            max: values[n - 1],
        })
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Metric {
    pub identity: Identity,
    pub summary: Summary,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Capture {
    pub run_id: String,
    pub directory: String,
    pub repetition: Repetition,
    pub conditions: Conditions,
    pub metrics: Vec<Metric>,
    pub schema_version: u32,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Pair {
    repetition: u32,
    baseline: f64,
    candidate: f64,
    delta: f64,
    delta_percent: Option<f64>,
}
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct PairedMetric {
    pub identity: Identity,
    pub baseline: Summary,
    pub candidate: Summary,
    pub delta: Summary,
    pub delta_percent: Option<Summary>,
    pub pairs: Vec<Pair>,
}
#[derive(Serialize)]
pub struct Comparison {
    pub comparable: bool,
    pub errors: Vec<String>,
    pub metrics: Vec<PairedMetric>,
}

pub fn validate_ismc_conditions(conditions: &Conditions) -> Result<()> {
    for &key in CONDITIONS {
        let value = conditions
            .get(key)
            .filter(|s| !s.trim().is_empty())
            .ok_or_else(|| format!("Missing comparability condition: {key}"))?;
        if !["mode", "visibility", "bounds", "custom_data", "rhi"].contains(&key)
            && !value.parse::<f64>().is_ok_and(f64::is_finite)
        {
            return Err(format!("Invalid numeric comparability condition: {key}").into());
        }
    }
    if conditions["result_schema"] != "1" {
        return Err("Unsupported SandboxISMC conditions schema.".into());
    }
    Ok(())
}

pub fn validate_ismc_request(conditions: &Conditions, request: &Request) -> Result<()> {
    for (key, expected) in request.build_comparison_conditions() {
        let actual = conditions
            .get(&key)
            .ok_or_else(|| format!("Missing comparability condition: {key}"))?;
        let equal = if let Ok(number) = expected.parse::<f64>() {
            actual.parse::<f64>().is_ok_and(|value| {
                value.is_finite() && (number - value).abs() <= 1e-6_f64.max(number.abs() * 1e-6)
            })
        } else {
            actual == &expected
        };
        if !equal {
            return Err(format!(
                "SandboxISMC condition mismatch: {key} requested {expected}, observed {actual}."
            )
            .into());
        }
    }
    Ok(())
}

pub fn read_ismc_metrics(path: &Path, conditions: &Conditions) -> Result<Vec<Metric>> {
    let content = fs::read_to_string(path)?;
    let mut lines = content.trim_start_matches('\u{feff}').lines();
    let header = parse_csv_row(lines.next().ok_or("Empty SandboxISMC CSV.")?)?;
    if header.iter().collect::<BTreeSet<_>>().len() != header.len() {
        return Err("Duplicate SandboxISMC CSV columns.".into());
    }
    let summaries = ["samples", "min", "median", "p95", "max"];
    for name in [
        "renderer",
        "metric",
        "unit",
        "mode",
        "instances",
        "visibility",
        "bounds",
        "custom_data",
        "update_percent",
        "churn",
        "min_instances",
        "half_cycle_updates",
        "replacement_percent",
        "warmup_updates",
        "warmup_seconds",
        "measurement_seconds",
        "updated_instances",
    ]
    .into_iter()
    .chain(summaries)
    {
        if !header.iter().any(|c| c == name) {
            return Err(format!("SandboxISMC CSV lacks {name}.").into());
        }
    }
    let mut metrics = Vec::new();
    for line in lines.filter(|s| !s.trim().is_empty()) {
        let row = parse_csv_row(line)?;
        if row.len() != header.len() {
            return Err("SandboxISMC CSV row has incorrect column count.".into());
        }
        let row: BTreeMap<_, _> = header.iter().map(String::as_str).zip(&row).collect();
        let value = |key: &str| row[key].as_str();
        let numeric = |key: &str| -> Result<f64> {
            value(key)
                .parse::<f64>()
                .ok()
                .filter(|v| v.is_finite())
                .ok_or_else(|| format!("Invalid CSV numeric value: {key}").into())
        };
        for (key, expected) in conditions {
            if let Some(actual) = row.get(key.as_str()) {
                let equal = if let Ok(number) = expected.parse::<f64>() {
                    (numeric(key)? - number).abs() <= 0.000501
                } else {
                    expected == *actual
                };
                if !equal {
                    return Err(format!("CSV/conditions mismatch: {key}").into());
                }
            }
        }
        let dimensions = row
            .iter()
            .filter(|(k, _)| !["metric", "unit"].contains(k) && !summaries.contains(k))
            .map(|(&k, &v)| (k.to_owned(), v.clone()))
            .collect();
        metrics.push(Metric {
            identity: Identity {
                metric: value("metric").into(),
                unit: value("unit").into(),
                dimensions,
            },
            summary: Summary {
                samples: value("samples").parse()?,
                min: numeric("min")?,
                median: numeric("median")?,
                p95: numeric("p95")?,
                max: numeric("max")?,
            },
        });
    }
    index_metrics(&metrics)?;
    Ok(metrics)
}

type MetricIndex<'a> = BTreeMap<(&'a str, &'a Conditions), &'a Metric>;

fn index_metrics(metrics: &[Metric]) -> Result<MetricIndex<'_>> {
    let mut map = BTreeMap::new();
    for metric in metrics {
        metric.summary.validate_summary()?;
        if metric.identity.metric.trim().is_empty() || metric.identity.unit.trim().is_empty() {
            return Err("Metric name and unit are required.".into());
        }
        let key = (metric.identity.metric.as_str(), &metric.identity.dimensions);
        if map.insert(key, metric).is_some() {
            return Err("Duplicate metric identity.".into());
        }
    }
    if map.is_empty() {
        return Err("Benchmark contains no metrics.".into());
    }
    Ok(map)
}

pub fn compare_captures(captures: &[Capture]) -> Result<Comparison> {
    let measured: Vec<_> = captures.iter().filter(|c| !c.repetition.warmup).collect();
    let reference = *measured
        .first()
        .ok_or("No complete measured repetitions.")?;
    // Reuse capture indexes across all metric comparisons.
    let mut pairs: BTreeMap<u32, BTreeMap<&str, MetricIndex<'_>>> = BTreeMap::new();
    for &capture in &measured {
        let id = capture.repetition.repetition;
        let side = capture.repetition.side.as_str();
        let pair = pairs.entry(id).or_default();
        if id == 0
            || !["baseline", "candidate"].contains(&side)
            || pair
                .insert(side, index_metrics(&capture.metrics)?)
                .is_some()
        {
            return Err(format!(
                "Repetition {id} requires exactly one measured baseline and one candidate."
            )
            .into());
        }
    }

    for (id, pair) in &pairs {
        if pair.len() != 2 {
            return Err(format!(
                "Repetition {id} requires exactly one measured baseline and one candidate."
            )
            .into());
        }
    }

    let expected = index_metrics(&reference.metrics)?;
    let mut errors = BTreeSet::new();
    for capture in &measured {
        for key in reference.conditions.keys().chain(capture.conditions.keys()) {
            if reference.conditions.get(key) != capture.conditions.get(key) {
                errors.insert(format!("Comparability mismatch: {key}"));
            }
        }
    }
    for actual in pairs.values().flat_map(BTreeMap::values) {
        for key in expected.keys().chain(actual.keys()) {
            match (expected.get(key), actual.get(key)) {
                (Some(a), Some(b)) if a.identity.unit != b.identity.unit => {
                    errors.insert(format!("Unit mismatch: {}", key.0));
                }
                (None, _) | (_, None) => {
                    errors.insert(format!("Missing metric or dimension mismatch: {}", key.0));
                }
                _ => {}
            }
        }
    }
    if !errors.is_empty() {
        return Ok(Comparison {
            comparable: false,
            errors: errors.into_iter().collect(),
            metrics: vec![],
        });
    }
    let mut metrics = Vec::new();
    for (key, metric) in expected {
        let mut values = Vec::new();
        for (&id, pair) in &pairs {
            let baseline = pair["baseline"][&key].summary.median;
            let candidate = pair["candidate"][&key].summary.median;
            let delta = candidate - baseline;
            let delta_percent = (baseline != 0.0).then(|| 100.0 * delta / baseline);
            if !delta.is_finite() || delta_percent.is_some_and(|v| !v.is_finite()) {
                return Err("Metric delta overflowed.".into());
            }
            values.push(Pair {
                repetition: id,
                baseline,
                candidate,
                delta,
                delta_percent,
            });
        }
        metrics.push(PairedMetric {
            identity: metric.identity.clone(),
            baseline: Summary::from_samples(values.iter().map(|v| v.baseline))?,
            candidate: Summary::from_samples(values.iter().map(|v| v.candidate))?,
            delta: Summary::from_samples(values.iter().map(|v| v.delta))?,
            delta_percent: if values.iter().all(|v| v.delta_percent.is_some()) {
                Some(Summary::from_samples(
                    values.iter().map(|v| v.delta_percent.unwrap()),
                )?)
            } else {
                None
            },
            pairs: values,
        });
    }
    Ok(Comparison {
        comparable: true,
        errors: vec![],
        metrics,
    })
}

pub fn write_ismc_comparison_reports(
    directory: &Path,
    manifest: &Value,
    plan: &Plan,
    captures: &[Capture],
) -> Result<bool> {
    let request = plan
        .ismc
        .as_ref()
        .ok_or("Comparison plan has no SandboxISMC workload.")?;
    let mut actual: Vec<_> = captures.iter().map(|c| c.repetition.clone()).collect();
    let mut expected = plan.sequence.clone();
    actual.sort_by_key(|r| r.sequence);
    expected.sort_by_key(|r| r.sequence);
    if actual != expected
        || captures
            .iter()
            .map(|c| &c.run_id)
            .collect::<BTreeSet<_>>()
            .len()
            != captures.len()
        || actual
            .iter()
            .map(|r| r.sequence)
            .collect::<BTreeSet<_>>()
            .len()
            != actual.len()
    {
        return Err("Incomplete comparison or duplicate capture identity.".into());
    }
    for capture in captures {
        if capture.schema_version != 1
            || capture.run_id.trim().is_empty()
            || capture.repetition.sequence == 0
            || capture.repetition.repetition == 0
            || !["baseline", "candidate"].contains(&capture.repetition.side.as_str())
        {
            return Err("Unsupported capture schema or invalid identity.".into());
        }
        validate_ismc_conditions(&capture.conditions)?;
        index_metrics(&capture.metrics)?;
    }
    let comparison = compare_captures(captures)?;
    if comparison.comparable {
        for capture in captures.iter().filter(|c| !c.repetition.warmup) {
            validate_ismc_request(&capture.conditions, request)?;
        }
    }
    let measured: Vec<_> = captures.iter().filter(|c| !c.repetition.warmup).collect();
    let conditions = &measured[0].conditions;
    let metadata = json!({"runId":manifest["runId"],"label":manifest["label"],"validationOnly":plan.validation_only,
        "baseline":plan.baseline,"candidate":plan.candidate,"repetitions":measured.len()/2,"sequence":expected,"conditions":conditions});
    write_json(
        &directory.join("comparison.json"),
        &json!({"schemaVersion":2,"metadata":metadata,
        "comparable":comparison.comparable,"errors":comparison.errors,"metrics":comparison.metrics}),
    )?;
    let mut report = String::from("# SandboxISMC revision comparison\n\n");
    report.push_str(&format!(
        "{}\n\n",
        escape_markdown_text(manifest["label"].as_str().unwrap_or(""))
    ));
    if plan.validation_only {
        report.push_str(
            "**Validation only — protocol/comparability smoke; no performance conclusions.**\n\n",
        );
    }
    report.push_str(&format!("Run: {}. Baseline: {} ({}). Candidate: {} ({}).\n\nMeasured repetitions per side: {}. Order: {}.\n\n",
        escape_markdown_text(manifest["runId"].as_str().unwrap_or("")),plan.baseline.commit,if plan.baseline.dirty {"dirty"} else {"clean"},
        plan.candidate.commit,if plan.candidate.dirty {"dirty"} else {"clean"},measured.len()/2,
        expected.iter().map(|r| format!("{}{}{}",if r.side=="baseline" {"A"} else {"B"},r.repetition,if r.warmup {" (warmup)"} else {""})).collect::<Vec<_>>().join(", ")));
    report.push_str("| Condition | Value |\n|---|---|\n");
    for (key, value) in conditions {
        report.push_str(&format!(
            "| {} | {} |\n",
            escape_markdown_text(key),
            escape_markdown_text(value)
        ));
    }
    let mut rows = vec![
        [
            "renderer",
            "metric",
            "unit",
            "baseline_runs",
            "candidate_runs",
            "baseline_run_median",
            "candidate_run_median",
            "paired_delta_median",
            "paired_delta_percent_median",
        ]
        .map(str::to_owned)
        .to_vec(),
    ];
    if comparison.comparable {
        report.push_str("\nValues summarize complete-run medians. Deltas are paired candidate-minus-baseline differences by repetition ID. Samples count independent repetitions; captures.json preserves within-run summaries. Medians average the middle pair; p95 uses nearest rank. Percent summaries are unavailable if any baseline is zero.\n\n| Renderer | Metric | Unit | Pairs | Baseline median | Candidate median | Paired delta median | Paired delta % median |\n|---|---|---|---:|---:|---:|---:|---:|\n");
        for item in &comparison.metrics {
            let renderer = item
                .identity
                .dimensions
                .get("renderer")
                .cloned()
                .unwrap_or_default();
            let percent = item
                .delta_percent
                .as_ref()
                .map(|s| s.median.to_string())
                .unwrap_or_default();
            report.push_str(&format!(
                "| {} | {} | {} | {} | {} | {} | {} | {} |\n",
                escape_markdown_text(&renderer),
                escape_markdown_text(&item.identity.metric),
                escape_markdown_text(&item.identity.unit),
                item.delta.samples,
                item.baseline.median,
                item.candidate.median,
                item.delta.median,
                percent
            ));
            rows.push(vec![
                renderer,
                item.identity.metric.clone(),
                item.identity.unit.clone(),
                item.baseline.samples.to_string(),
                item.candidate.samples.to_string(),
                item.baseline.median.to_string(),
                item.candidate.median.to_string(),
                item.delta.median.to_string(),
                percent,
            ]);
        }
    } else {
        report.push_str("\nIncomparable. No performance deltas were calculated.\n");
        for error in &comparison.errors {
            report.push_str(&format!("- {}\n", escape_markdown_text(error)));
        }
    }
    write_text(&directory.join("comparison.md"), report)?;
    write_csv(&directory.join("comparison.csv"), &rows)?;
    Ok(comparison.comparable)
}

fn escape_markdown_text(value: &str) -> String {
    value
        .replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('|', "\\|")
        .replace('*', "\\*")
        .replace(['\r', '\n'], " ")
}

#[cfg(test)]
#[path = "../tests/results/mod.rs"]
mod tests;
