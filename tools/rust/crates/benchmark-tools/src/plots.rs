mod kernel;
mod native;

pub use kernel::plot_kernel;
pub use native::{plot_comparison, plot_fighters};

use crate::cli::{PlotKind, PlotOptions};
use crate::support::*;
use plotters::coord::ranged1d::{DefaultFormatting, KeyPointHint};
use plotters::coord::types::RangedCoordf64;
use plotters::prelude::*;
use serde_json::Value;
use std::path::{Path, PathBuf};

const COLORS: [RGBColor; 10] = [
    RGBColor(31, 119, 180),
    RGBColor(255, 127, 14),
    RGBColor(44, 160, 44),
    RGBColor(214, 39, 40),
    RGBColor(148, 103, 189),
    RGBColor(140, 86, 75),
    RGBColor(227, 119, 194),
    RGBColor(100, 100, 100),
    RGBColor(160, 150, 20),
    RGBColor(23, 160, 180),
];

pub fn run(args: &PlotOptions) -> Result<()> {
    if args.kind != PlotKind::Kernel && args.baseline.is_some() {
        return Err("--baseline is only supported for kernel plots.".into());
    }
    let input: Value = read_json(&args.input)?;
    let output = &args.output_dir;
    let paths = match args.kind {
        PlotKind::Kernel => {
            plot_kernel(&input, output, args.baseline.as_deref().unwrap_or("scalar"))?
        }
        PlotKind::Fighter => plot_fighters(&input, output)?,
        PlotKind::Comparison => {
            if input["schemaVersion"] != 1 {
                return Err("Unsupported native comparison schema.".into());
            }
            let comparison = serde_json::from_value(input["comparison"].clone())?;
            plot_comparison(&comparison, output)?
        }
    };
    print_paths(&paths);
    Ok(())
}

fn print_paths(paths: &[PathBuf]) {
    for path in paths {
        println!("Plot: {}", path.display());
    }
}

pub fn automatic(render: impl FnOnce() -> Result<Vec<PathBuf>>) -> Vec<PathBuf> {
    match render() {
        Ok(paths) => {
            print_paths(&paths);
            paths
        }
        Err(error) => {
            eprintln!(
                "warning: plotting failed: {error}. Saved benchmark results remain available; regenerate with 'coj benchmark plot'."
            );
            Vec::new()
        }
    }
}

struct Point {
    x: f64,
    y: f64,
    deviation: f64,
}

struct Series {
    label: String,
    points: Vec<Point>,
}

struct Panel {
    label: String,
    series: Vec<Series>,
    reference: Option<f64>,
}

// Keep explicit tick positions on floating axes; Plotters' WithKeyPoints<f64>
// does not implement ValueFormatter, which configure_mesh requires.
struct TickAxis {
    inner: RangedCoordf64,
    ticks: Vec<f64>,
}

impl TickAxis {
    fn new(range: std::ops::Range<f64>, ticks: Vec<f64>) -> Self {
        Self {
            inner: range.into(),
            ticks,
        }
    }
}

impl Ranged for TickAxis {
    type ValueType = f64;
    type FormatOption = DefaultFormatting;

    fn range(&self) -> std::ops::Range<f64> {
        self.inner.range()
    }

    fn map(&self, value: &f64, limit: (i32, i32)) -> i32 {
        self.inner.map(value, limit)
    }

    fn key_points<Hint: KeyPointHint>(&self, hint: Hint) -> Vec<f64> {
        if hint.weight().allow_light_points() {
            Vec::new()
        } else {
            self.ticks.clone()
        }
    }
}

fn range(values: impl Iterator<Item = f64>) -> Result<std::ops::Range<f64>> {
    let mut low = f64::INFINITY;
    let mut high = f64::NEG_INFINITY;
    for value in values {
        if !value.is_finite() {
            return Err("Plot values must be finite.".into());
        }
        low = low.min(value);
        high = high.max(value);
    }
    if !low.is_finite() {
        return Err("No points to plot.".into());
    }
    let padding = if low == high {
        low.abs().max(1.0) * 0.05
    } else {
        (high - low) * 0.06
    };
    Ok((low - padding)..(high + padding))
}

fn line_plot(
    path: &Path,
    title: &str,
    x_label: &str,
    logarithmic: bool,
    panels: &[Panel],
) -> Result<()> {
    let mut svg = String::new();
    {
        let legend_rows = panels
            .iter()
            .map(|panel| panel.series.len().div_ceil(3))
            .max()
            .unwrap_or(0);
        let panel_height = 340 + legend_rows as u32 * 24;
        let root =
            SVGBackend::with_string(&mut svg, (1200, 55 + panel_height * panels.len() as u32))
                .into_drawing_area();
        root.fill(&WHITE)?;
        let root = root.titled(title, ("sans-serif", 23))?;
        for (area, panel) in root.split_evenly((panels.len(), 1)).into_iter().zip(panels) {
            // Transform positions while keeping tick labels at measured counts.
            let transform = |x: f64| if logarithmic { x.log2() } else { x };
            let mut ticks: Vec<_> = panel
                .series
                .iter()
                .flat_map(|s| s.points.iter().map(|p| transform(p.x)))
                .collect();
            ticks.sort_by(f64::total_cmp);
            ticks.dedup();
            let stride = ticks.len().div_ceil(8).max(1);
            let ticks = ticks.into_iter().step_by(stride).collect();
            let x_range = range(
                panel
                    .series
                    .iter()
                    .flat_map(|s| s.points.iter().map(|p| transform(p.x))),
            )?;
            let y_range = range(
                panel
                    .series
                    .iter()
                    .flat_map(|s| {
                        s.points
                            .iter()
                            .flat_map(|p| [p.y - p.deviation, p.y + p.deviation])
                    })
                    .chain(panel.reference),
            )?;
            let (plot_area, legend) = area.split_vertically(330);
            let mut chart = ChartBuilder::on(&plot_area)
                .margin(15)
                .x_label_area_size(45)
                .y_label_area_size(90)
                .build_cartesian_2d(TickAxis::new(x_range, ticks), y_range)?;
            let format_x = |x: &f64| {
                if logarithmic {
                    format!("{:.0}", 2.0_f64.powf(*x))
                } else {
                    format!("{x:.0}")
                }
            };
            chart
                .configure_mesh()
                .x_desc(x_label)
                .y_desc(&panel.label)
                .x_label_formatter(&format_x)
                .x_labels(8)
                .light_line_style(RGBColor(230, 230, 230))
                .draw()?;
            if let Some(reference) = panel.reference {
                let x = chart.as_coord_spec().get_x_range();
                chart.draw_series(LineSeries::new(
                    [(x.start, reference), (x.end, reference)],
                    BLACK.mix(0.4),
                ))?;
            }
            for (index, series) in panel.series.iter().enumerate() {
                let color = COLORS[index % COLORS.len()];
                chart.draw_series(LineSeries::new(
                    series.points.iter().map(|p| (transform(p.x), p.y)),
                    color.stroke_width(2),
                ))?;
                chart.draw_series(series.points.iter().filter(|p| p.deviation > 0.0).map(|p| {
                    ErrorBar::new_vertical(
                        transform(p.x),
                        p.y - p.deviation,
                        p.y,
                        p.y + p.deviation,
                        color,
                        6,
                    )
                }))?;
                chart.draw_series(
                    series
                        .points
                        .iter()
                        .map(|p| Circle::new((transform(p.x), p.y), 3, color.filled())),
                )?;
                let x = 25 + (index % 3) as i32 * 395;
                let y = 15 + (index / 3) as i32 * 24;
                legend.draw(&PathElement::new(
                    [(x, y), (x + 20, y)],
                    color.stroke_width(3),
                ))?;
                legend.draw(&Text::new(
                    series.label.as_str(),
                    (x + 27, y + 5),
                    ("sans-serif", 15),
                ))?;
            }
        }
        root.present()?;
    }
    write_text(path, svg)
}

struct BarRow {
    label: String,
    values: Vec<f64>,
}

fn bar_plot(
    path: &Path,
    title: &str,
    axis_label: &str,
    labels: &[&str],
    rows: &[BarRow],
) -> Result<()> {
    let mut svg = String::new();
    {
        let x_range = range(
            rows.iter()
                .flat_map(|row| row.values.iter().copied())
                .chain([0.0]),
        )?;
        let height = 150 + rows.len() as u32 * 75;
        let root = SVGBackend::with_string(&mut svg, (1200, height)).into_drawing_area();
        root.fill(&WHITE)?;
        let (plot_area, legend) = root.split_vertically(height - 35);
        let mut chart = ChartBuilder::on(&plot_area)
            .caption(title, ("sans-serif", 23))
            .margin(25)
            .x_label_area_size(65)
            .y_label_area_size(320)
            .build_cartesian_2d(
                x_range,
                TickAxis::new(
                    rows.len() as f64..0.0,
                    (0..rows.len()).map(|i| i as f64 + 0.5).collect(),
                ),
            )?;
        let format_y = |y: &f64| {
            let index = y.floor() as usize;
            rows.get(index)
                .map_or(String::new(), |row| row.label.clone())
        };
        chart
            .configure_mesh()
            .disable_y_mesh()
            .y_labels(rows.len())
            .y_label_formatter(&format_y)
            .x_desc(axis_label)
            .draw()?;
        for (series_index, label) in labels.iter().enumerate() {
            let color = COLORS[series_index];
            chart.draw_series(rows.iter().enumerate().filter_map(|(index, row)| {
                row.values.get(series_index).map(|value| {
                    let start =
                        index as f64 + 0.1 + 0.8 * series_index as f64 / labels.len() as f64;
                    Rectangle::new(
                        [(0.0, start), (*value, start + 0.7 / labels.len() as f64)],
                        color.filled(),
                    )
                })
            }))?;
            let x = 340 + series_index as i32 * 220;
            legend.draw(&Rectangle::new([(x, 5), (x + 18, 16)], color.filled()))?;
            legend.draw(&Text::new(*label, (x + 25, 17), ("sans-serif", 16)))?;
        }
        root.present()?;
    }
    write_text(path, svg)
}

#[cfg(test)]
#[path = "../tests/plots/mod.rs"]
mod tests;
