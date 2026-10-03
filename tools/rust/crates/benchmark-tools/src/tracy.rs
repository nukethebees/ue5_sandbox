use crate::cli::{TracyOptions, TracySort};
use crate::support::{Result, write_json};
use serde::Serialize;
use std::{
    collections::BTreeMap,
    io::{self, BufRead, BufReader, Read},
    path::PathBuf,
    process::{Command, Stdio},
};

const SEPARATOR: char = '\u{1f}';
const MAX_ZONES: usize = 4096;

#[derive(Serialize)]
struct Selection {
    filter: String,
    from_seconds: f64,
    to_seconds: Option<f64>,
    thread: Option<u64>,
    self_time: bool,
}

#[derive(Eq, Ord, PartialEq, PartialOrd, Serialize)]
struct Zone {
    name: String,
    file: String,
    line: u32,
}

#[derive(Serialize)]
struct Invocation {
    start_ns: i64,
    duration_ns: u64,
    thread: u64,
}

#[derive(Default)]
struct Samples {
    durations: Vec<u64>,
    worst: Vec<Invocation>,
}

#[derive(Serialize)]
struct ZoneReport {
    #[serde(flatten)]
    zone: Zone,
    count: usize,
    total_ns: u64,
    mean_ns: f64,
    min_ns: u64,
    p50_ns: u64,
    p95_ns: u64,
    p99_ns: u64,
    max_ns: u64,
    worst: Vec<Invocation>,
}

#[derive(Serialize)]
struct Report {
    schema_version: u32,
    trace: PathBuf,
    trace_bytes: u64,
    exporter_version: String,
    selection: Selection,
    percentile_method: &'static str,
    sort: TracySort,
    matched_events: usize,
    skipped_incomplete_events: usize,
    matched_zones: usize,
    omitted_zones: usize,
    zones: Vec<ZoneReport>,
}

fn summarize_probe_events(
    reader: impl Read,
    selection: &Selection,
    max_events: usize,
    worst_count: usize,
) -> Result<(Vec<ZoneReport>, usize, usize)> {
    let mut reader = BufReader::new(reader);
    let mut line = String::new();
    reader.read_line(&mut line)?;
    let expected = [
        "name",
        "src_file",
        "src_line",
        "ns_since_start",
        "exec_time_ns",
        "thread",
        "value",
    ]
    .join(&SEPARATOR.to_string());
    if line.trim_end_matches(['\r', '\n']) != expected {
        return Err(
            "Unexpected Tracy event header; use a matching tracy-csvexport version.".into(),
        );
    }

    let mut samples = BTreeMap::<Zone, Samples>::new();
    let mut matched_events = 0;
    let mut incomplete_events = 0;
    let mut row_number = 1;
    loop {
        line.clear();
        if reader.read_line(&mut line)? == 0 {
            break;
        }
        row_number += 1;
        let fields: Vec<_> = line
            .trim_end_matches(['\r', '\n'])
            .splitn(7, SEPARATOR)
            .collect();
        if fields.len() != 7 {
            return Err(format!("Malformed Tracy event at row {row_number}; embedded newlines in zone names/text are unsupported.").into());
        }
        let start_ns: i64 = fields[3]
            .parse()
            .map_err(|_| format!("Invalid start time at row {row_number}"))?;
        let duration: i64 = fields[4]
            .parse()
            .map_err(|_| format!("Invalid duration at row {row_number}"))?;
        let thread: u64 = fields[5]
            .parse()
            .map_err(|_| format!("Invalid thread at row {row_number}"))?;
        let start_seconds = start_ns as f64 / 1_000_000_000.0;
        if !fields[0].contains(&selection.filter)
            || start_seconds < selection.from_seconds
            || selection.to_seconds.is_some_and(|end| start_seconds >= end)
            || selection.thread.is_some_and(|id| id != thread)
        {
            continue;
        }
        if duration < 0 {
            incomplete_events += 1;
            continue;
        }
        if matched_events == max_events {
            return Err("Tracy event limit exceeded; narrow --filter/time window or increase --max-events. No partial report was published.".into());
        }
        let zone = Zone {
            name: fields[0].into(),
            file: fields[1].into(),
            line: fields[2]
                .parse()
                .map_err(|_| format!("Invalid source line at row {row_number}"))?,
        };
        if samples.len() == MAX_ZONES && !samples.contains_key(&zone) {
            return Err("Tracy zone limit exceeded (4096); narrow --filter. No partial report was published.".into());
        }
        let sample = samples.entry(zone).or_default();
        let duration_ns = duration as u64;
        sample.durations.push(duration_ns);
        if worst_count != 0 {
            sample.worst.push(Invocation {
                start_ns,
                duration_ns,
                thread,
            });
            sample.worst.sort_by(|a, b| {
                b.duration_ns
                    .cmp(&a.duration_ns)
                    .then(a.start_ns.cmp(&b.start_ns))
                    .then(a.thread.cmp(&b.thread))
            });
            sample.worst.truncate(worst_count);
        }
        matched_events += 1;
    }

    let mut zones = Vec::with_capacity(samples.len());
    for (zone, mut sample) in samples {
        sample.durations.sort_unstable();
        let count = sample.durations.len();
        let total_ns = sample
            .durations
            .iter()
            .try_fold(0_u64, |sum, value| sum.checked_add(*value))
            .ok_or("Total zone duration overflow")?;
        let percentile = |percent: usize| sample.durations[(count * percent).div_ceil(100) - 1];
        zones.push(ZoneReport {
            zone,
            count,
            total_ns,
            mean_ns: total_ns as f64 / count as f64,
            min_ns: sample.durations[0],
            p50_ns: percentile(50),
            p95_ns: percentile(95),
            p99_ns: percentile(99),
            max_ns: sample.durations[count - 1],
            worst: sample.worst,
        });
    }
    Ok((zones, matched_events, incomplete_events))
}

pub fn generate_tracy_report(args: &TracyOptions) -> Result<()> {
    let trace = std::fs::canonicalize(&args.trace)?;
    let trace_bytes = std::fs::metadata(&trace)?.len();
    let output = args.output.as_ref().map(std::path::absolute).transpose()?;
    if let Some(path) = &output
        && std::fs::canonicalize(path).ok().as_ref() == Some(&trace)
    {
        return Err("The output must not overwrite the input trace.".into());
    }
    let selection = Selection {
        filter: args.filter.clone(),
        from_seconds: args.from_seconds,
        to_seconds: args.to_seconds,
        thread: args.thread,
        self_time: args.self_time,
    };
    if selection
        .to_seconds
        .is_some_and(|end| end <= selection.from_seconds)
    {
        return Err("--to-seconds must be greater than --from-seconds.".into());
    }
    let sort = args.sort;
    let top = args.top as usize;
    let worst = args.worst as usize;
    let max_events = args.max_events as usize;

    let version = Command::new("tracy-csvexport")
        .arg("--version")
        .output()
        .map_err(|error| format!("Cannot launch tracy-csvexport from PATH: {error}"))?;
    if !version.status.success() {
        return Err("tracy-csvexport --version failed.".into());
    }
    let exporter_version = format!(
        "{}{}",
        String::from_utf8_lossy(&version.stdout),
        String::from_utf8_lossy(&version.stderr)
    )
    .trim()
    .to_owned();
    let mut command = Command::new("tracy-csvexport");
    command.args(["-u", "-c", "-s", &SEPARATOR.to_string()]);
    if !selection.filter.is_empty() {
        command.args(["-f", &selection.filter]);
    }
    if selection.self_time {
        command.arg("-e");
    }
    let mut child = command
        .arg(&trace)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()?;
    let stderr = child.stderr.take().ok_or("Missing exporter stderr")?;
    // Drain diagnostics concurrently so the exporter cannot block on a full pipe.
    let diagnostics = std::thread::spawn(move || -> io::Result<String> {
        let mut reader = BufReader::new(stderr);
        let mut retained = Vec::new();
        reader.by_ref().take(8192).read_to_end(&mut retained)?;
        io::copy(&mut reader, &mut io::sink())?;
        Ok(String::from_utf8_lossy(&retained).into_owned())
    });
    let result = summarize_probe_events(
        child.stdout.take().ok_or("Missing exporter stdout")?,
        &selection,
        max_events,
        worst,
    );
    if result.is_err() {
        let _ = child.kill();
    }
    let status = child.wait()?;
    let diagnostics = diagnostics
        .join()
        .map_err(|_| "Exporter diagnostic reader failed")??;
    let (mut zones, matched_events, skipped_incomplete_events) =
        result.map_err(|error| format!("{error}\nExporter diagnostics: {diagnostics}"))?;
    if !status.success() {
        return Err(format!("tracy-csvexport failed ({status}): {diagnostics}").into());
    }
    let metric = |zone: &ZoneReport| match sort {
        TracySort::Max => zone.max_ns as f64,
        TracySort::Mean => zone.mean_ns,
        TracySort::P95 => zone.p95_ns as f64,
        TracySort::Total => zone.total_ns as f64,
    };
    zones.sort_by(|a, b| metric(b).total_cmp(&metric(a)).then(a.zone.cmp(&b.zone)));
    let matched_zones = zones.len();
    zones.truncate(top);
    let report = Report {
        schema_version: 1,
        trace,
        trace_bytes,
        exporter_version,
        selection,
        percentile_method: "exact_nearest_rank",
        sort,
        matched_events,
        skipped_incomplete_events,
        matched_zones,
        omitted_zones: matched_zones - zones.len(),
        zones,
    };
    if let Some(output) = output {
        write_json(&output, &report)?;
        println!(
            "Tracy report: {} ({} events, {} zones, {} omitted)",
            output.display(),
            report.matched_events,
            report.zones.len(),
            report.omitted_zones
        );
    } else {
        println!("{}", serde_json::to_string_pretty(&report)?);
    }
    Ok(())
}
