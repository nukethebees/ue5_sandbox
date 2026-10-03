use super::*;
use crate::results::Summary;
use crate::time::TimeUnit;
use serde::Deserialize;
use std::collections::{BTreeMap, BTreeSet};

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
struct Key {
    operation: String,
    policy: String,
    layout: String,
    backend: String,
    values: String,
    alignment: String,
    count: u64,
}

struct Measurement {
    key: Key,
    time: f64,
    deviation: f64,
}

#[derive(Default)]
struct Samples {
    iterations: Vec<f64>,
    median: Option<f64>,
    mean: Option<f64>,
    deviation: f64,
}

#[derive(Default, Deserialize)]
#[serde(rename_all = "snake_case")]
enum RunType {
    #[default]
    Iteration,
    Aggregate,
}

#[derive(PartialEq, Deserialize)]
#[serde(rename_all = "snake_case")]
enum AggregateKind {
    Median,
    Mean,
    Stddev,
    #[serde(other)]
    Other,
}

#[derive(Default, PartialEq, Deserialize)]
#[serde(rename_all = "snake_case")]
enum AggregateUnit {
    #[default]
    Time,
    #[serde(other)]
    Other,
}

fn parse_key(name: &str) -> Result<Key> {
    let parts: Vec<_> = name.split('/').collect();
    if parts.len() != 8 || parts[7] != "real_time" || parts.iter().any(|part| part.is_empty()) {
        return Err(format!("Invalid kernel run_name: {name}").into());
    }
    let count = parts[6]
        .parse::<u64>()
        .ok()
        .filter(|count| *count > 0)
        .ok_or_else(|| format!("Invalid kernel element count: {name}"))?;
    Ok(Key {
        operation: parts[0].into(),
        policy: parts[1].into(),
        layout: parts[2].into(),
        backend: parts[3].into(),
        values: parts[4].into(),
        alignment: parts[5].into(),
        count,
    })
}

fn load(document: &Value) -> Result<Vec<Measurement>> {
    let entries = document["benchmarks"]
        .as_array()
        .ok_or("Kernel JSON must contain a benchmarks array.")?;
    let mut groups = BTreeMap::<Key, Samples>::new();
    for entry in entries {
        let name = entry["run_name"]
            .as_str()
            .ok_or("Missing kernel run_name.")?;
        if entry["error_occurred"] == true {
            eprintln!(
                "warning: skipped {name}: {}",
                entry["error_message"]
                    .as_str()
                    .unwrap_or("benchmark failed")
            );
            continue;
        }
        let key = parse_key(name)?;
        let run_type = entry
            .get("run_type")
            .map(RunType::deserialize)
            .transpose()?
            .unwrap_or_default();
        let aggregate = match run_type {
            RunType::Iteration => None,
            RunType::Aggregate => {
                let kind = AggregateKind::deserialize(&entry["aggregate_name"])?;
                let unit = entry
                    .get("aggregate_unit")
                    .map(AggregateUnit::deserialize)
                    .transpose()?
                    .unwrap_or_default();
                if unit != AggregateUnit::Time || kind == AggregateKind::Other {
                    continue;
                }
                Some(kind)
            }
        };
        let unit = TimeUnit::deserialize(&entry["time_unit"])?;
        let time = unit.to_nanoseconds(read_finite_number(entry, "/real_time")?);
        if !time.is_finite()
            || time < 0.0
            || (time == 0.0 && aggregate != Some(AggregateKind::Stddev))
        {
            return Err("Kernel timing must be positive; standard deviation may be zero.".into());
        }
        let samples = groups.entry(key).or_default();
        match aggregate {
            None => samples.iterations.push(time),
            Some(AggregateKind::Median) => samples.median = Some(time),
            Some(AggregateKind::Mean) => samples.mean = Some(time),
            Some(AggregateKind::Stddev) => samples.deviation = time,
            Some(AggregateKind::Other) => unreachable!(),
        }
    }
    let mut measurements = Vec::new();
    for (key, samples) in groups {
        let (time, deviation) = if samples.iterations.is_empty() {
            (
                samples
                    .median
                    .or(samples.mean)
                    .ok_or("Kernel result has no iterations, median or mean.")?,
                samples.deviation,
            )
        } else {
            let summary = Summary::from_samples(samples.iterations.iter().copied())?;
            let count = samples.iterations.len();
            let mean = samples.iterations.iter().sum::<f64>() / count as f64;
            let deviation = if count > 1 {
                (samples
                    .iterations
                    .iter()
                    .map(|value| (value - mean).powi(2))
                    .sum::<f64>()
                    / (count - 1) as f64)
                    .sqrt()
            } else {
                0.0
            };
            (summary.median, deviation)
        };
        measurements.push(Measurement {
            key,
            time,
            deviation,
        });
    }
    if measurements.is_empty() {
        return Err("Kernel JSON contains no successful measurements.".into());
    }
    Ok(measurements)
}

fn slug(value: &str) -> String {
    value
        .split(|c: char| !c.is_ascii_alphanumeric())
        .filter(|part| !part.is_empty())
        .collect::<Vec<_>>()
        .join("-")
        .to_ascii_lowercase()
}

fn series(
    points: &[&Measurement],
    label: impl Fn(&Key) -> String,
    value: impl Fn(&Measurement) -> (f64, f64),
) -> Vec<Series> {
    let mut groups = BTreeMap::<String, Vec<Point>>::new();
    for point in points {
        let (y, deviation) = value(point);
        groups.entry(label(&point.key)).or_default().push(Point {
            x: point.key.count as f64,
            y,
            deviation,
        });
    }
    groups
        .into_iter()
        .map(|(label, mut points)| {
            points.sort_by(|a, b| a.x.total_cmp(&b.x));
            Series { label, points }
        })
        .collect()
}

fn ratios(
    points: &[&Measurement],
    baseline: impl Fn(&Key) -> bool,
    comparison: impl Fn(&Key) -> bool,
    label: impl Fn(&Key) -> String,
    invert: bool,
) -> Vec<Series> {
    // Callers select the varying dimension; labels retain all other matching dimensions.
    let baseline: BTreeMap<_, _> = points
        .iter()
        .filter(|p| baseline(&p.key))
        .map(|p| ((label(&p.key), p.key.count), p.time))
        .collect();
    let mut groups = BTreeMap::<String, Vec<Point>>::new();
    for point in points.iter().filter(|p| comparison(&p.key)) {
        let name = label(&point.key);
        if let Some(time) = baseline.get(&(name.clone(), point.key.count)) {
            let y = if invert {
                point.time / time
            } else {
                time / point.time
            };
            groups.entry(name).or_default().push(Point {
                x: point.key.count as f64,
                y,
                deviation: 0.0,
            });
        }
    }
    groups
        .into_iter()
        .map(|(label, mut points)| {
            points.sort_by(|a, b| a.x.total_cmp(&b.x));
            Series { label, points }
        })
        .collect()
}

pub fn plot_kernel(document: &Value, output: &Path, baseline: &str) -> Result<Vec<PathBuf>> {
    let measurements = load(document)?;
    let mut groups = BTreeMap::new();
    for point in &measurements {
        groups
            .entry((&point.key.operation, &point.key.policy, &point.key.values))
            .or_insert_with(Vec::new)
            .push(point);
    }
    let mut written = Vec::new();
    for ((operation, policy, values), points) in groups {
        let prefix = format!("{}-{}-{}", slug(operation), slug(policy), slug(values));
        let title = format!("{operation} ({policy}): {values}");
        let alignments: BTreeSet<_> = points.iter().map(|p| p.key.alignment.as_str()).collect();
        let layouts: BTreeSet<_> = points.iter().map(|p| p.key.layout.as_str()).collect();
        for alignment in &alignments {
            let aligned: Vec<_> = points
                .iter()
                .copied()
                .filter(|p| p.key.alignment == *alignment)
                .collect();
            let path = output.join(format!("{prefix}-{}-all-throughput.svg", slug(alignment)));
            line_plot(
                &path,
                &format!("{title}, {alignment}"),
                "Element count (log2 scale)",
                true,
                &[Panel {
                    label: "Throughput (elements/s)".into(),
                    series: series(
                        &aligned,
                        |k| format!("{}/{}", k.layout, k.backend),
                        |p| (p.key.count as f64 * 1e9 / p.time, 0.0),
                    ),
                    reference: None,
                }],
            )?;
            written.push(path);
            for layout in &layouts {
                let selected: Vec<_> = aligned
                    .iter()
                    .copied()
                    .filter(|p| p.key.layout == *layout)
                    .collect();
                if selected.is_empty() {
                    continue;
                }
                let mut panels = vec![
                    Panel {
                        label: "Throughput (billion elements/s)".into(),
                        series: series(
                            &selected,
                            |k| k.backend.clone(),
                            |p| (p.key.count as f64 / p.time, 0.0),
                        ),
                        reference: None,
                    },
                    Panel {
                        label: "Time (ns/element; sample stddev)".into(),
                        series: series(
                            &selected,
                            |k| k.backend.clone(),
                            |p| {
                                (
                                    p.time / p.key.count as f64,
                                    p.deviation / p.key.count as f64,
                                )
                            },
                        ),
                        reference: None,
                    },
                ];
                let effective = if selected.iter().any(|p| p.key.backend == baseline) {
                    baseline
                } else {
                    "autovec-avx2"
                };
                let times: BTreeMap<_, _> = selected
                    .iter()
                    .filter(|p| p.key.backend == effective)
                    .map(|p| (p.key.count, p.time))
                    .collect();
                if times.is_empty() {
                    eprintln!(
                        "warning: no {baseline} or autovec-avx2 baseline for {title}, {layout}, {alignment}; omitting speedup."
                    );
                } else {
                    let paired: Vec<_> = selected
                        .iter()
                        .copied()
                        .filter(|p| times.contains_key(&p.key.count))
                        .collect();
                    panels.push(Panel {
                        label: format!("Speedup over {effective}"),
                        series: series(
                            &paired,
                            |k| k.backend.clone(),
                            |p| (times[&p.key.count] / p.time, 0.0),
                        ),
                        reference: Some(1.0),
                    });
                }
                let path = output.join(format!(
                    "{}-{}-{}-{}-{}.svg",
                    slug(operation),
                    slug(policy),
                    slug(layout),
                    slug(values),
                    slug(alignment)
                ));
                line_plot(
                    &path,
                    &format!("{title}, {layout}, {alignment}"),
                    "Element count (log2 scale)",
                    true,
                    &panels,
                )?;
                written.push(path);
            }
        }
        for layout in &layouts {
            let selected: Vec<_> = points
                .iter()
                .copied()
                .filter(|p| p.key.layout == *layout)
                .collect();
            let series = ratios(
                &selected,
                |k| k.alignment == "aligned",
                |k| k.alignment == "unaligned",
                |k| k.backend.clone(),
                true,
            );
            if !series.is_empty() {
                let path = output.join(format!(
                    "{}-{}-{}-{}-alignment-penalty.svg",
                    slug(operation),
                    slug(policy),
                    slug(layout),
                    slug(values)
                ));
                line_plot(
                    &path,
                    &format!("{title}, {layout}"),
                    "Element count (log2 scale)",
                    true,
                    &[Panel {
                        label: "Unaligned/aligned time ratio".into(),
                        series,
                        reference: Some(1.0),
                    }],
                )?;
                written.push(path);
            }
        }
        for (base, others, suffix) in [
            ("flat", &["chunked16"][..], "layout-speedup"),
            (
                "aos",
                &["soa-flat", "soa-chunked16"][..],
                "aos-layout-speedup",
            ),
        ] {
            let selected: Vec<_> = points
                .iter()
                .copied()
                .filter(|p| p.key.alignment == "aligned")
                .collect();
            let mut all_series = Vec::new();
            for other in others {
                let mut series = ratios(
                    &selected,
                    |k| k.layout == base,
                    |k| k.layout == *other,
                    |k| k.backend.clone(),
                    false,
                );
                for item in &mut series {
                    item.label = format!("{other}/{}", item.label);
                }
                all_series.extend(series);
            }
            if !all_series.is_empty() {
                let path = output.join(format!("{prefix}-{suffix}.svg"));
                line_plot(
                    &path,
                    &title,
                    "Element count (log2 scale)",
                    true,
                    &[Panel {
                        label: format!("Speedup over {base} (aligned)"),
                        series: all_series,
                        reference: Some(1.0),
                    }],
                )?;
                written.push(path);
            }
        }
    }
    Ok(written)
}

#[cfg(test)]
#[path = "../../tests/plots/kernel.rs"]
mod tests;
