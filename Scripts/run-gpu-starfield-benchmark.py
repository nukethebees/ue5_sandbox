from __future__ import annotations

import argparse
import csv
import math
import statistics
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path


METRICS = {
    "game_thread_ms": "GameThreadTime",
    "render_thread_ms": "RenderThreadTime",
    "gpu_ms": "GPUTime",
    "translucency_gpu_ms": "GPU/Translucency",
    "starfield_submit_cpu_ms": "Exclusive/AllWorkers/GpuStarfieldSubmit",
    "rhi_draw_calls": "RHI/DrawCalls",
    "translucency_draw_calls": "DrawCall/Translucency",
    "primitives_drawn": "RHI/PrimitivesDrawn",
}


@dataclass(frozen=True)
class CaptureSummary:
    star_count: int
    repeat_index: int
    enabled: bool
    frame_count: int
    medians: dict[str, float]


@dataclass(frozen=True)
class BenchmarkDelta:
    star_count: int
    metric: str
    disabled_median: float
    enabled_median: float
    delta_median: float
    delta_min: float
    delta_max: float


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run and validate the automated GPU starfield A/B benchmark."
    )
    parser.add_argument("--editor", required=True, type=Path)
    parser.add_argument("--project", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--counts", default="10000,100000,1000000")
    parser.add_argument("--warmup-frames", type=int, default=60)
    parser.add_argument("--capture-frames", type=int, default=180)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--trim-frames", type=int, default=10)
    parser.add_argument("--timeout-seconds", type=int, default=1800)
    return parser.parse_args()


def parse_counts(value: str) -> list[int]:
    counts = list(dict.fromkeys(int(entry.strip()) for entry in value.split(",")))
    if not counts or any(count <= 0 or count > 1_000_000 for count in counts):
        raise ValueError("counts must be unique values in the range 1..1,000,000")
    return counts


def run_editor(args: argparse.Namespace, raw_directory: Path, log_path: Path) -> None:
    command = [
        str(args.editor.resolve()),
        str(args.project.resolve()),
        "/SandboxShaders/Showcase/SandboxShaders_Showcase",
        "-game",
        "-RenderOffscreen",
        "-unattended",
        "-nop4",
        "-nosplash",
        "-nosound",
        "-stdout",
        "-ResX=1280",
        "-ResY=720",
        "-windowed",
        "-benchmark",
        "-deterministic",
        "-fps=60",
        "-csvGpuStats",
        "-GpuStarfieldBenchmark",
        f"-GpuStarfieldBenchmarkCounts={args.counts}",
        f"-GpuStarfieldBenchmarkWarmupFrames={args.warmup_frames}",
        f"-GpuStarfieldBenchmarkCaptureFrames={args.capture_frames}",
        f"-GpuStarfieldBenchmarkRepeats={args.repeats}",
        f"-GpuStarfieldBenchmarkOutput={raw_directory.resolve()}",
        "-ExecCmds=r.VSync 0;r.ScreenPercentage 100;r.DynamicRes.OperationMode 0",
    ]

    with log_path.open("w", encoding="utf-8", errors="replace") as log_file:
        result = subprocess.run(
            command,
            cwd=args.project.resolve().parent,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            timeout=args.timeout_seconds,
            check=False,
        )

    if result.returncode != 0:
        log_tail = log_path.read_text(encoding="utf-8", errors="replace").splitlines()[-80:]
        raise RuntimeError(
            f"Unreal benchmark exited with code {result.returncode}.\n"
            + "\n".join(log_tail)
        )


def read_capture(
    path: Path,
    star_count: int,
    repeat_index: int,
    enabled: bool,
    trim_frames: int,
) -> CaptureSummary:
    with path.open("r", encoding="utf-8-sig", errors="replace", newline="") as csv_file:
        reader = csv.reader(csv_file)
        try:
            header = next(reader)
        except StopIteration as error:
            raise RuntimeError(f"Capture is empty: {path}") from error

        indices = {name: header.index(column) for name, column in METRICS.items() if column in header}
        if "game_thread_ms" not in indices:
            raise RuntimeError(f"Capture has no GameThreadTime column: {path}")

        values: dict[str, list[float]] = {name: [] for name in METRICS}
        for row in reader:
            game_thread_index = indices["game_thread_ms"]
            if game_thread_index >= len(row):
                continue
            try:
                game_thread_value = float(row[game_thread_index])
            except ValueError:
                continue
            if not math.isfinite(game_thread_value):
                continue

            for name, index in indices.items():
                if index >= len(row) or not row[index]:
                    continue
                try:
                    value = float(row[index])
                except ValueError:
                    continue
                if math.isfinite(value):
                    values[name].append(value)

    frame_count = len(values["game_thread_ms"])
    if frame_count <= trim_frames * 2:
        raise RuntimeError(f"Capture has only {frame_count} frames: {path}")

    medians: dict[str, float] = {}
    for name, samples in values.items():
        if not samples:
            medians[name] = 0.0
            continue
        trimmed = samples[trim_frames:-trim_frames]
        medians[name] = statistics.median(trimmed) if trimmed else 0.0

    return CaptureSummary(star_count, repeat_index, enabled, frame_count, medians)


def load_captures(
    raw_directory: Path,
    counts: list[int],
    repeats: int,
    trim_frames: int,
) -> list[CaptureSummary]:
    captures: list[CaptureSummary] = []
    for star_count in counts:
        for repeat_index in range(1, repeats + 1):
            for enabled in (False, True):
                state = "enabled" if enabled else "disabled"
                path = raw_directory / f"gpu_starfield_{star_count}_{state}_r{repeat_index}.csv"
                if not path.is_file():
                    raise RuntimeError(f"Expected capture was not produced: {path}")
                captures.append(
                    read_capture(path, star_count, repeat_index, enabled, trim_frames)
                )
    return captures


def calculate_deltas(
    captures: list[CaptureSummary], counts: list[int], repeats: int
) -> list[BenchmarkDelta]:
    by_key = {
        (capture.star_count, capture.repeat_index, capture.enabled): capture
        for capture in captures
    }
    result: list[BenchmarkDelta] = []
    for star_count in counts:
        for metric in METRICS:
            disabled_values: list[float] = []
            enabled_values: list[float] = []
            deltas: list[float] = []
            for repeat_index in range(1, repeats + 1):
                disabled = by_key[(star_count, repeat_index, False)].medians[metric]
                enabled = by_key[(star_count, repeat_index, True)].medians[metric]
                disabled_values.append(disabled)
                enabled_values.append(enabled)
                deltas.append(enabled - disabled)
            result.append(
                BenchmarkDelta(
                    star_count=star_count,
                    metric=metric,
                    disabled_median=statistics.median(disabled_values),
                    enabled_median=statistics.median(enabled_values),
                    delta_median=statistics.median(deltas),
                    delta_min=min(deltas),
                    delta_max=max(deltas),
                )
            )
    return result


def validate_results(deltas: list[BenchmarkDelta], counts: list[int]) -> None:
    by_key = {(delta.star_count, delta.metric): delta for delta in deltas}
    errors: list[str] = []
    for star_count in counts:
        draw_delta = by_key[(star_count, "translucency_draw_calls")].delta_median
        if not 0.5 <= draw_delta <= 1.5:
            errors.append(
                f"{star_count:,} stars changed translucency draws by {draw_delta:.2f}, expected 1"
            )

        primitive_delta = by_key[(star_count, "primitives_drawn")].delta_median
        expected_primitives = star_count * 2
        tolerance = max(expected_primitives * 0.02, 4.0)
        if abs(primitive_delta - expected_primitives) > tolerance:
            errors.append(
                f"{star_count:,} stars changed primitives by {primitive_delta:.0f}, "
                f"expected {expected_primitives:,}"
            )

        submit_time = by_key[(star_count, "starfield_submit_cpu_ms")].enabled_median
        if submit_time <= 0.0:
            errors.append(f"{star_count:,} stars did not record the mesh submission scope")

    if errors:
        raise RuntimeError("Benchmark validation failed:\n- " + "\n- ".join(errors))


def write_csv(path: Path, deltas: list[BenchmarkDelta]) -> None:
    with path.open("w", encoding="utf-8", newline="") as output_file:
        writer = csv.writer(output_file)
        writer.writerow(
            [
                "star_count",
                "metric",
                "disabled_median",
                "enabled_median",
                "delta_median",
                "delta_min",
                "delta_max",
            ]
        )
        for delta in deltas:
            writer.writerow(
                [
                    delta.star_count,
                    delta.metric,
                    f"{delta.disabled_median:.6f}",
                    f"{delta.enabled_median:.6f}",
                    f"{delta.delta_median:.6f}",
                    f"{delta.delta_min:.6f}",
                    f"{delta.delta_max:.6f}",
                ]
            )


def write_markdown(path: Path, deltas: list[BenchmarkDelta], counts: list[int]) -> str:
    by_key = {(delta.star_count, delta.metric): delta for delta in deltas}
    lines = [
        "# GPU starfield isolated A/B benchmark",
        "",
        "Values are medians of paired enabled-minus-disabled captures. The range is the minimum",
        "and maximum paired delta across repeats. CPU totals remain whole-frame deltas; the",
        "starfield submission scope measures only `GetDynamicMeshElements`, which UE schedules on a worker.",
        "",
        "| Stars | GT delta ms | RT delta ms | Submit CPU ms | GPU delta ms | Translucency GPU delta ms | Draw delta | Primitive delta |",
        "| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for star_count in counts:
        game_thread = metric_for(by_key, star_count, "game_thread_ms")
        render_thread = metric_for(by_key, star_count, "render_thread_ms")
        submit = metric_for(by_key, star_count, "starfield_submit_cpu_ms")
        gpu = metric_for(by_key, star_count, "gpu_ms")
        translucency_gpu = metric_for(by_key, star_count, "translucency_gpu_ms")
        translucency_draws = metric_for(by_key, star_count, "translucency_draw_calls")
        primitives = metric_for(by_key, star_count, "primitives_drawn")
        lines.append(
            f"| {star_count:,} "
            f"| {game_thread.delta_median:.4f} "
            f"| {render_thread.delta_median:.4f} "
            f"| {submit.enabled_median:.4f} "
            f"| {gpu.delta_median:.4f} "
            f"| {translucency_gpu.delta_median:.4f} "
            f"| {translucency_draws.delta_median:.1f} "
            f"| {primitives.delta_median:.0f} |"
        )

    lines.extend(["", "Paired delta ranges:", ""])
    for star_count in counts:
        lines.append(f"- {star_count:,} stars:")
        for name in (
            "game_thread_ms",
            "render_thread_ms",
            "gpu_ms",
            "translucency_gpu_ms",
        ):
            delta = metric_for(by_key, star_count, name)
            lines.append(
                f"  - `{name}`: {delta.delta_median:.4f} ms "
                f"[{delta.delta_min:.4f}, {delta.delta_max:.4f}]"
            )

    report = "\n".join(lines) + "\n"
    path.write_text(report, encoding="utf-8")
    return report


def metric_for(
    by_key: dict[tuple[int, str], BenchmarkDelta], star_count: int, name: str
) -> BenchmarkDelta:
    return by_key[(star_count, name)]


def main() -> int:
    args = parse_arguments()
    counts = parse_counts(args.counts)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    run_directory = args.output.resolve() / timestamp
    raw_directory = run_directory / "raw"
    raw_directory.mkdir(parents=True, exist_ok=False)
    log_path = run_directory / "unreal.log"

    run_editor(args, raw_directory, log_path)
    captures = load_captures(raw_directory, counts, args.repeats, args.trim_frames)
    deltas = calculate_deltas(captures, counts, args.repeats)
    validate_results(deltas, counts)

    write_csv(run_directory / "GpuStarfieldBenchmark.csv", deltas)
    report = write_markdown(run_directory / "GpuStarfieldBenchmark.md", deltas, counts)
    (args.output.resolve() / "latest.txt").write_text(str(run_directory), encoding="utf-8")
    print(report)
    print(f"Artifacts: {run_directory}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"GPU starfield benchmark failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
