# /// script
# requires-python = ">=3.11"
# dependencies = [
#     "matplotlib>=3.10,<4",
# ]
# ///

from __future__ import annotations

import argparse
import importlib
import json
import math
import statistics
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any, TypeAlias, cast


TIME_UNIT_TO_NS = {
    "ns": 1.0,
    "us": 1_000.0,
    "ms": 1_000_000.0,
    "s": 1_000_000_000.0,
}

JsonValue: TypeAlias = (
    None | bool | int | float | str | list["JsonValue"] | dict[str, "JsonValue"]
)


@dataclass(frozen=True, order=True)
class BenchmarkKey:
    operation: str
    layout: str
    backend: str
    value_set: str
    alignment: str
    count: int


@dataclass(frozen=True)
class Measurement:
    time_ns: float


@dataclass(frozen=True)
class BenchmarkPoint:
    key: BenchmarkKey
    median_time_ns: float
    time_stddev_ns: float

    @property
    def ns_per_element(self) -> float:
        return self.median_time_ns / self.key.count

    @property
    def giga_elements_per_second(self) -> float:
        return self.key.count / self.median_time_ns


@dataclass(frozen=True)
class LoadedBenchmarks:
    points: tuple[BenchmarkPoint, ...]
    skipped: tuple[str, ...]


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Plot Google Benchmark JSON produced by the native kernel benchmarks."
    )
    parser.add_argument("results", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--baseline", default="scalar")
    parser.add_argument("--format", choices=("svg", "png"), default="png")
    return parser.parse_args()


def parse_benchmark_key(run_name: str) -> BenchmarkKey:
    parts = run_name.split("/")
    if len(parts) != 7 or parts[-1] != "real_time":
        raise ValueError(
            "benchmark run_name must be "
            "OPERATION/LAYOUT/BACKEND/VALUE_SET/ALIGNMENT/COUNT/real_time: "
            f"{run_name!r}"
        )

    try:
        count = int(parts[5])
    except ValueError as error:
        raise ValueError(f"benchmark count is not an integer: {run_name!r}") from error
    if count <= 0:
        raise ValueError(f"benchmark count must be positive: {run_name!r}")

    return BenchmarkKey(
        operation=parts[0],
        layout=parts[1],
        backend=parts[2],
        value_set=parts[3],
        alignment=parts[4],
        count=count,
    )


def read_measurement(entry: dict[str, JsonValue]) -> Measurement:
    time_unit = entry.get("time_unit")
    if time_unit not in TIME_UNIT_TO_NS:
        raise ValueError(f"unsupported benchmark time_unit: {time_unit!r}")

    real_time = entry.get("real_time")
    if not isinstance(real_time, (int, float)) or isinstance(real_time, bool):
        raise ValueError("benchmark real_time must be a number")
    time_ns = float(real_time) * TIME_UNIT_TO_NS[time_unit]
    if not math.isfinite(time_ns) or time_ns <= 0.0:
        raise ValueError("benchmark real_time must be finite and positive")

    return Measurement(time_ns=time_ns)


def load_benchmarks(path: Path) -> LoadedBenchmarks:
    try:
        document: JsonValue = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"failed to read benchmark JSON {path}: {error}") from error

    if not isinstance(document, dict) or not isinstance(document.get("benchmarks"), list):
        raise ValueError("benchmark JSON must contain a benchmarks array")
    benchmark_entries = document["benchmarks"]
    assert isinstance(benchmark_entries, list)

    iterations: dict[BenchmarkKey, list[Measurement]] = defaultdict(list)
    aggregates: dict[BenchmarkKey, dict[str, Measurement]] = defaultdict(dict)
    skipped: list[str] = []

    for entry_index, untyped_entry in enumerate(benchmark_entries):
        if not isinstance(untyped_entry, dict):
            raise ValueError(f"benchmark entry {entry_index} must be an object")
        entry = untyped_entry

        run_name = entry.get("run_name")
        if not isinstance(run_name, str):
            raise ValueError(f"benchmark entry {entry_index} has no string run_name")

        if entry.get("error_occurred") is True:
            message = entry.get("error_message", "unknown benchmark error")
            skipped.append(f"{run_name}: {message}")
            continue

        key = parse_benchmark_key(run_name)
        run_type = entry.get("run_type", "iteration")
        if run_type == "iteration":
            iterations[key].append(read_measurement(entry))
            continue
        if run_type != "aggregate":
            raise ValueError(f"unsupported benchmark run_type: {run_type!r}")

        aggregate_name = entry.get("aggregate_name")
        if not isinstance(aggregate_name, str):
            raise ValueError(f"aggregate benchmark entry has no aggregate_name: {run_name!r}")
        if entry.get("aggregate_unit", "time") == "time":
            aggregates[key][aggregate_name] = read_measurement(entry)

    points: list[BenchmarkPoint] = []
    keys = sorted(set(iterations) | set(aggregates))
    for key in keys:
        samples = iterations.get(key, [])
        if samples:
            sample_times = [sample.time_ns for sample in samples]
            median_time = statistics.median(sample_times)
            stddev_time = statistics.stdev(sample_times) if len(sample_times) > 1 else 0.0
        else:
            key_aggregates = aggregates[key]
            selected = key_aggregates.get("median") or key_aggregates.get("mean")
            if selected is None:
                raise ValueError(f"benchmark has no iteration, median, or mean result: {key}")
            median_time = selected.time_ns
            stddev = key_aggregates.get("stddev")
            stddev_time = stddev.time_ns if stddev is not None else 0.0

        points.append(
            BenchmarkPoint(
                key=key,
                median_time_ns=median_time,
                time_stddev_ns=stddev_time,
            )
        )

    if not points:
        raise ValueError("benchmark JSON contains no successful measurements")

    return LoadedBenchmarks(points=tuple(points), skipped=tuple(skipped))


def slugify(value: str) -> str:
    slug = "".join(character if character.isalnum() else "-" for character in value.lower())
    return "-".join(part for part in slug.split("-") if part)


def configure_axis(axis: Any, ylabel: str) -> None:
    axis.set_xscale("log", base=2)
    axis.set_xlabel("Element count")
    axis.set_ylabel(ylabel)
    axis.grid(True, which="both", alpha=0.25)


def load_pyplot() -> Any:
    matplotlib = cast(Any, importlib.import_module("matplotlib"))
    matplotlib.use("Agg")
    return cast(Any, importlib.import_module("matplotlib.pyplot"))


def write_backend_comparison(
    points: list[BenchmarkPoint],
    output_path: Path,
    baseline: str,
    title: str,
) -> None:
    plt = load_pyplot()

    figure, axes = plt.subplots(3, 1, figsize=(11, 12), sharex=True)
    baseline_times = {
        point.key.count: point.median_time_ns
        for point in points
        if point.key.backend == baseline
    }
    if not baseline_times:
        raise ValueError(f"baseline backend {baseline!r} is absent from {title}")

    backends = sorted({point.key.backend for point in points})
    for backend in backends:
        backend_points = sorted(
            (point for point in points if point.key.backend == backend),
            key=lambda point: point.key.count,
        )
        counts = [point.key.count for point in backend_points]
        axes[0].plot(
            counts,
            [point.giga_elements_per_second for point in backend_points],
            marker="o",
            label=backend,
        )
        axes[1].errorbar(
            counts,
            [point.ns_per_element for point in backend_points],
            yerr=[point.time_stddev_ns / point.key.count for point in backend_points],
            marker="o",
            capsize=2,
            label=backend,
        )

        comparable_points = [
            point for point in backend_points if point.key.count in baseline_times
        ]
        axes[2].plot(
            [point.key.count for point in comparable_points],
            [
                baseline_times[point.key.count] / point.median_time_ns
                for point in comparable_points
            ],
            marker="o",
            label=backend,
        )

    configure_axis(axes[0], "Throughput (billion elements/s)")
    configure_axis(axes[1], "Time (ns/element)")
    configure_axis(axes[2], f"Speedup over {baseline}")
    axes[2].axhline(1.0, color="black", linewidth=1.0, alpha=0.5)
    axes[0].legend(ncols=2)
    figure.suptitle(title)
    figure.tight_layout()
    figure.savefig(output_path, bbox_inches="tight")
    plt.close(figure)


def write_alignment_penalty(
    points: list[BenchmarkPoint], output_path: Path, title: str
) -> None:
    plt = load_pyplot()

    figure, axis = plt.subplots(figsize=(11, 6))
    backends = sorted({point.key.backend for point in points})
    plotted = False
    for backend in backends:
        aligned = {
            point.key.count: point.median_time_ns
            for point in points
            if point.key.backend == backend and point.key.alignment == "aligned"
        }
        unaligned = {
            point.key.count: point.median_time_ns
            for point in points
            if point.key.backend == backend and point.key.alignment == "unaligned"
        }
        counts = sorted(set(aligned) & set(unaligned))
        if not counts:
            continue
        plotted = True
        axis.plot(
            counts,
            [unaligned[count] / aligned[count] for count in counts],
            marker="o",
            label=backend,
        )

    if not plotted:
        plt.close(figure)
        raise ValueError(f"no aligned/unaligned pairs are available for {title}")

    configure_axis(axis, "Unaligned/aligned time ratio")
    axis.axhline(1.0, color="black", linewidth=1.0, alpha=0.5)
    axis.legend(ncols=2)
    figure.suptitle(title)
    figure.tight_layout()
    figure.savefig(output_path, bbox_inches="tight")
    plt.close(figure)


def write_layout_speedup(
    points: list[BenchmarkPoint], output_path: Path, title: str
) -> None:
    plt = load_pyplot()

    figure, axis = plt.subplots(figsize=(11, 6))
    backends = sorted({point.key.backend for point in points})
    plotted = False
    for backend in backends:
        flat = {
            point.key.count: point.median_time_ns
            for point in points
            if point.key.backend == backend
            and point.key.layout == "flat"
            and point.key.alignment == "aligned"
        }
        soaos = {
            point.key.count: point.median_time_ns
            for point in points
            if point.key.backend == backend
            and point.key.layout == "soaos16"
            and point.key.alignment == "aligned"
        }
        counts = sorted(set(flat) & set(soaos))
        if not counts:
            continue
        plotted = True
        axis.plot(
            counts,
            [flat[count] / soaos[count] for count in counts],
            marker="o",
            label=backend,
        )

    if not plotted:
        plt.close(figure)
        raise ValueError(f"no flat/soaos16 pairs are available for {title}")

    configure_axis(axis, "SoAoS16 speedup over flat")
    axis.axhline(1.0, color="black", linewidth=1.0, alpha=0.5)
    axis.legend(ncols=2)
    figure.suptitle(title)
    figure.tight_layout()
    figure.savefig(output_path, bbox_inches="tight")
    plt.close(figure)


def write_plots(
    loaded: LoadedBenchmarks,
    output_directory: Path,
    baseline: str,
    file_format: str,
) -> tuple[Path, ...]:
    output_directory.mkdir(parents=True, exist_ok=True)
    written: list[Path] = []
    operations = sorted({point.key.operation for point in loaded.points})

    for operation in operations:
        operation_points = [
            point for point in loaded.points if point.key.operation == operation
        ]
        layouts = sorted({point.key.layout for point in operation_points})
        for layout in layouts:
            layout_points = [
                point for point in operation_points if point.key.layout == layout
            ]
            value_sets = sorted({point.key.value_set for point in layout_points})
            for value_set in value_sets:
                value_points = [
                    point for point in layout_points if point.key.value_set == value_set
                ]
                alignments = sorted({point.key.alignment for point in value_points})
                for alignment in alignments:
                    comparison_points = [
                        point for point in value_points if point.key.alignment == alignment
                    ]
                    output_path = output_directory / (
                        f"{slugify(operation)}-{slugify(layout)}-{slugify(value_set)}-"
                        f"{slugify(alignment)}.{file_format}"
                    )
                    write_backend_comparison(
                        comparison_points,
                        output_path,
                        baseline,
                        f"{operation}: {layout}, {value_set}, {alignment}",
                    )
                    written.append(output_path)

                if "aligned" in alignments and "unaligned" in alignments:
                    alignment_path = output_directory / (
                        f"{slugify(operation)}-{slugify(layout)}-{slugify(value_set)}-"
                        f"alignment-penalty.{file_format}"
                    )
                    write_alignment_penalty(
                        value_points,
                        alignment_path,
                        f"{operation}: {layout}, {value_set} alignment penalty",
                    )
                    written.append(alignment_path)

        if "flat" in layouts and "soaos16" in layouts:
            value_sets = sorted({point.key.value_set for point in operation_points})
            for value_set in value_sets:
                value_points = [
                    point
                    for point in operation_points
                    if point.key.value_set == value_set
                ]
                layout_path = output_directory / (
                    f"{slugify(operation)}-{slugify(value_set)}-layout-speedup."
                    f"{file_format}"
                )
                write_layout_speedup(
                    value_points,
                    layout_path,
                    f"{operation}: {value_set} SoAoS16 speedup over flat",
                )
                written.append(layout_path)

    return tuple(written)


def main() -> int:
    args = parse_arguments()
    try:
        loaded = load_benchmarks(args.results)
        written = write_plots(loaded, args.output, args.baseline, args.format)
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    for skipped in loaded.skipped:
        print(f"warning: skipped {skipped}", file=sys.stderr)
    print(f"Wrote {len(written)} plots to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
