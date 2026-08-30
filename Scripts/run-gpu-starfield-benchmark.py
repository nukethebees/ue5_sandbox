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
class BenchmarkConfiguration:
    width: int
    height: int
    star_size_multiplier: float

    @property
    def name(self) -> str:
        size = f"{self.star_size_multiplier:g}".replace(".", "p")
        return f"{self.width}x{self.height}_size{size}"


@dataclass(frozen=True)
class CaptureSummary:
    configuration: BenchmarkConfiguration
    star_count: int
    repeat_index: int
    enabled: bool
    camera_motion: bool
    frame_count: int
    medians: dict[str, float]
    minima: dict[str, float]
    maxima: dict[str, float]


@dataclass(frozen=True)
class BenchmarkDelta:
    configuration: BenchmarkConfiguration
    star_count: int
    camera_motion: bool
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
    parser.add_argument("--resolutions", default="1280x720")
    parser.add_argument("--size-multipliers", default="1")
    parser.add_argument("--camera-modes", default="stationary,moving")
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


def parse_resolutions(value: str) -> list[tuple[int, int]]:
    resolutions: list[tuple[int, int]] = []
    for entry in value.split(","):
        dimensions = entry.strip().lower().split("x")
        if len(dimensions) != 2:
            raise ValueError("resolutions must use WIDTHxHEIGHT entries")
        resolution = (int(dimensions[0]), int(dimensions[1]))
        if not 320 <= resolution[0] <= 7680 or not 200 <= resolution[1] <= 4320:
            raise ValueError("resolutions must be between 320x200 and 7680x4320")
        if resolution not in resolutions:
            resolutions.append(resolution)
    if not resolutions:
        raise ValueError("at least one resolution is required")
    return resolutions


def parse_size_multipliers(value: str) -> list[float]:
    multipliers = list(dict.fromkeys(float(entry.strip()) for entry in value.split(",")))
    if not multipliers or any(
        not math.isfinite(multiplier) or multiplier < 0.0 or multiplier > 100.0
        for multiplier in multipliers
    ):
        raise ValueError("size multipliers must be unique values in the range 0..100")
    return multipliers


def parse_camera_modes(value: str) -> list[bool]:
    names = list(dict.fromkeys(entry.strip().lower() for entry in value.split(",")))
    if not names or any(name not in {"stationary", "moving"} for name in names):
        raise ValueError("camera modes must contain stationary, moving, or both")
    return [name == "moving" for name in names]


def make_configurations(args: argparse.Namespace) -> list[BenchmarkConfiguration]:
    return [
        BenchmarkConfiguration(width, height, multiplier)
        for width, height in parse_resolutions(args.resolutions)
        for multiplier in parse_size_multipliers(args.size_multipliers)
    ]


def run_editor(
    args: argparse.Namespace,
    configuration: BenchmarkConfiguration,
    camera_modes: list[bool],
    raw_directory: Path,
    log_path: Path,
) -> None:
    camera_mode_names = ",".join("moving" if mode else "stationary" for mode in camera_modes)
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
        f"-ResX={configuration.width}",
        f"-ResY={configuration.height}",
        "-ForceRes",
        "-windowed",
        "-benchmark",
        "-deterministic",
        "-fps=60",
        "-csvGpuStats",
        "-GpuStarfieldBenchmark",
        f"-GpuStarfieldBenchmarkCounts={args.counts}",
        f"-GpuStarfieldBenchmarkStarSizeMultiplier={configuration.star_size_multiplier}",
        f"-GpuStarfieldBenchmarkCameraModes={camera_mode_names}",
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
    configuration: BenchmarkConfiguration,
    star_count: int,
    repeat_index: int,
    enabled: bool,
    camera_motion: bool,
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
        actual_width = 0
        actual_height = 0
        for row in reader:
            for metadata_name, target in (
                ("[systemresolution.resx]", "width"),
                ("[systemresolution.resy]", "height"),
            ):
                if metadata_name not in row:
                    continue
                metadata_index = row.index(metadata_name)
                if metadata_index + 1 >= len(row):
                    continue
                try:
                    value = int(row[metadata_index + 1])
                except ValueError:
                    continue
                if target == "width":
                    actual_width = value
                else:
                    actual_height = value

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
    if (actual_width, actual_height) != (configuration.width, configuration.height):
        raise RuntimeError(
            f"Capture rendered at {actual_width}x{actual_height}, expected "
            f"{configuration.width}x{configuration.height}: {path}"
        )

    medians: dict[str, float] = {}
    minima: dict[str, float] = {}
    maxima: dict[str, float] = {}
    for name, samples in values.items():
        if not samples:
            medians[name] = 0.0
            minima[name] = 0.0
            maxima[name] = 0.0
            continue
        trimmed = samples[trim_frames:-trim_frames]
        medians[name] = statistics.median(trimmed) if trimmed else 0.0
        minima[name] = min(trimmed) if trimmed else 0.0
        maxima[name] = max(trimmed) if trimmed else 0.0

    return CaptureSummary(
        configuration,
        star_count,
        repeat_index,
        enabled,
        camera_motion,
        frame_count,
        medians,
        minima,
        maxima,
    )


def load_captures(
    raw_directory: Path,
    configuration: BenchmarkConfiguration,
    counts: list[int],
    camera_modes: list[bool],
    repeats: int,
    trim_frames: int,
) -> list[CaptureSummary]:
    captures: list[CaptureSummary] = []
    for star_count in counts:
        for repeat_index in range(1, repeats + 1):
            for camera_motion in camera_modes:
                motion = "moving" if camera_motion else "stationary"
                for enabled in (False, True):
                    state = "enabled" if enabled else "disabled"
                    path = (
                        raw_directory
                        / f"gpu_starfield_{star_count}_{motion}_{state}_r{repeat_index}.csv"
                    )
                    if not path.is_file():
                        raise RuntimeError(f"Expected capture was not produced: {path}")
                    captures.append(
                        read_capture(
                            path,
                            configuration,
                            star_count,
                            repeat_index,
                            enabled,
                            camera_motion,
                            trim_frames,
                        )
                    )
    return captures


def calculate_deltas(
    captures: list[CaptureSummary],
    configurations: list[BenchmarkConfiguration],
    counts: list[int],
    camera_modes: list[bool],
    repeats: int,
) -> list[BenchmarkDelta]:
    by_key = {
        (
            capture.configuration,
            capture.star_count,
            capture.repeat_index,
            capture.enabled,
            capture.camera_motion,
        ): capture
        for capture in captures
    }
    result: list[BenchmarkDelta] = []
    for configuration in configurations:
        for star_count in counts:
            for camera_motion in camera_modes:
                for metric in METRICS:
                    disabled_values: list[float] = []
                    enabled_values: list[float] = []
                    deltas: list[float] = []
                    for repeat_index in range(1, repeats + 1):
                        disabled = by_key[
                            (configuration, star_count, repeat_index, False, camera_motion)
                        ].medians[metric]
                        enabled = by_key[
                            (configuration, star_count, repeat_index, True, camera_motion)
                        ].medians[metric]
                        disabled_values.append(disabled)
                        enabled_values.append(enabled)
                        deltas.append(enabled - disabled)
                    result.append(
                        BenchmarkDelta(
                            configuration=configuration,
                            star_count=star_count,
                            camera_motion=camera_motion,
                            metric=metric,
                            disabled_median=statistics.median(disabled_values),
                            enabled_median=statistics.median(enabled_values),
                            delta_median=statistics.median(deltas),
                            delta_min=min(deltas),
                            delta_max=max(deltas),
                        )
                    )
    return result


def validate_results(
    captures: list[CaptureSummary],
    deltas: list[BenchmarkDelta],
    configurations: list[BenchmarkConfiguration],
    counts: list[int],
    camera_modes: list[bool],
) -> None:
    by_key = {
        (delta.configuration, delta.star_count, delta.camera_motion, delta.metric): delta
        for delta in deltas
    }
    errors: list[str] = []
    for configuration in configurations:
        for star_count in counts:
            for camera_motion in camera_modes:
                mode = "moving" if camera_motion else "stationary"
                label = f"{configuration.name}, {star_count:,} {mode} stars"
                draw_delta = by_key[
                    (configuration, star_count, camera_motion, "translucency_draw_calls")
                ].delta_median
                if not 0.5 <= draw_delta <= 1.5:
                    errors.append(
                        f"{label} changed translucency draws by {draw_delta:.2f}, expected 1"
                    )

                primitive_delta = by_key[
                    (configuration, star_count, camera_motion, "primitives_drawn")
                ].delta_median
                expected_primitives = star_count * 2
                tolerance = max(expected_primitives * 0.02, 4.0)
                if abs(primitive_delta - expected_primitives) > tolerance:
                    errors.append(
                        f"{label} changed primitives by {primitive_delta:.0f}, "
                        f"expected {expected_primitives:,}"
                    )

                submit_time = by_key[
                    (configuration, star_count, camera_motion, "starfield_submit_cpu_ms")
                ].enabled_median
                if submit_time <= 0.0:
                    errors.append(f"{label} did not record the mesh submission scope")

            expected_primitives = star_count * 2
            moving_captures = [
                capture
                for capture in captures
                if capture.configuration == configuration
                and capture.star_count == star_count
                and capture.camera_motion
                and capture.enabled
            ]
            for capture in moving_captures:
                if capture.minima["primitives_drawn"] < expected_primitives:
                    errors.append(
                        f"{configuration.name}, {star_count:,} moving stars disappeared "
                        f"during repeat {capture.repeat_index}"
                    )

    if errors:
        raise RuntimeError("Benchmark validation failed:\n- " + "\n- ".join(errors))


def write_csv(path: Path, deltas: list[BenchmarkDelta]) -> None:
    with path.open("w", encoding="utf-8", newline="") as output_file:
        writer = csv.writer(output_file)
        writer.writerow(
            [
                "width",
                "height",
                "star_size_multiplier",
                "star_count",
                "camera_motion",
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
                    delta.configuration.width,
                    delta.configuration.height,
                    f"{delta.configuration.star_size_multiplier:g}",
                    delta.star_count,
                    "moving" if delta.camera_motion else "stationary",
                    delta.metric,
                    f"{delta.disabled_median:.6f}",
                    f"{delta.enabled_median:.6f}",
                    f"{delta.delta_median:.6f}",
                    f"{delta.delta_min:.6f}",
                    f"{delta.delta_max:.6f}",
                ]
            )


def write_markdown(
    path: Path,
    deltas: list[BenchmarkDelta],
    configurations: list[BenchmarkConfiguration],
    counts: list[int],
    camera_modes: list[bool],
) -> str:
    by_key = {
        (delta.configuration, delta.star_count, delta.camera_motion, delta.metric): delta
        for delta in deltas
    }
    lines = [
        "# GPU starfield isolated A/B benchmark",
        "",
        "Values are medians of paired enabled-minus-disabled captures. The range is the minimum",
        "and maximum paired delta across repeats. CPU totals remain whole-frame deltas; the",
        "starfield submission scope measures only `GetDynamicMeshElements`, which UE schedules on a worker.",
    ]
    if True in camera_modes:
        lines.append("Moving captures translate the camera 10,000 km along a deterministic curved path.")
    lines.extend(
        [
            "",
            "| Resolution | Size | Stars | Camera | GT delta ms | RT delta ms | Submit CPU ms | GPU delta ms | Translucency GPU delta ms | Draw delta | Primitive delta |",
            "| :--- | ---: | ---: | :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
        ]
    )
    for configuration in configurations:
        for star_count in counts:
            for camera_motion in camera_modes:
                mode = "Moving" if camera_motion else "Stationary"
                game_thread = metric_for(
                    by_key, configuration, star_count, camera_motion, "game_thread_ms"
                )
                render_thread = metric_for(
                    by_key, configuration, star_count, camera_motion, "render_thread_ms"
                )
                submit = metric_for(
                    by_key,
                    configuration,
                    star_count,
                    camera_motion,
                    "starfield_submit_cpu_ms",
                )
                gpu = metric_for(
                    by_key, configuration, star_count, camera_motion, "gpu_ms"
                )
                translucency_gpu = metric_for(
                    by_key,
                    configuration,
                    star_count,
                    camera_motion,
                    "translucency_gpu_ms",
                )
                translucency_draws = metric_for(
                    by_key,
                    configuration,
                    star_count,
                    camera_motion,
                    "translucency_draw_calls",
                )
                primitives = metric_for(
                    by_key, configuration, star_count, camera_motion, "primitives_drawn"
                )
                lines.append(
                    f"| {configuration.width}x{configuration.height} "
                    f"| {configuration.star_size_multiplier:g} "
                    f"| {star_count:,} "
                    f"| {mode} "
                    f"| {game_thread.delta_median:.4f} "
                    f"| {render_thread.delta_median:.4f} "
                    f"| {submit.enabled_median:.4f} "
                    f"| {gpu.delta_median:.4f} "
                    f"| {translucency_gpu.delta_median:.4f} "
                    f"| {translucency_draws.delta_median:.1f} "
                    f"| {primitives.delta_median:.0f} |"
                )

    lines.extend(["", "Paired delta ranges:", ""])
    for configuration in configurations:
        for star_count in counts:
            for camera_motion in camera_modes:
                mode = "moving" if camera_motion else "stationary"
                lines.append(
                    f"- {configuration.width}x{configuration.height}, size "
                    f"{configuration.star_size_multiplier:g}, {star_count:,} stars, {mode} camera:"
                )
                for name in (
                    "game_thread_ms",
                    "render_thread_ms",
                    "gpu_ms",
                    "translucency_gpu_ms",
                ):
                    delta = metric_for(
                        by_key, configuration, star_count, camera_motion, name
                    )
                    lines.append(
                        f"  - `{name}`: {delta.delta_median:.4f} ms "
                        f"[{delta.delta_min:.4f}, {delta.delta_max:.4f}]"
                    )

    report = "\n".join(lines) + "\n"
    path.write_text(report, encoding="utf-8")
    return report


def metric_for(
    by_key: dict[
        tuple[BenchmarkConfiguration, int, bool, str], BenchmarkDelta
    ],
    configuration: BenchmarkConfiguration,
    star_count: int,
    camera_motion: bool,
    name: str,
) -> BenchmarkDelta:
    return by_key[(configuration, star_count, camera_motion, name)]


def main() -> int:
    args = parse_arguments()
    counts = parse_counts(args.counts)
    configurations = make_configurations(args)
    camera_modes = parse_camera_modes(args.camera_modes)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    run_directory = args.output.resolve() / timestamp
    run_directory.mkdir(parents=True, exist_ok=False)

    captures: list[CaptureSummary] = []
    for configuration in configurations:
        configuration_directory = run_directory / configuration.name
        raw_directory = configuration_directory / "raw"
        raw_directory.mkdir(parents=True)
        log_path = configuration_directory / "unreal.log"
        print(f"Running {configuration.name}...")
        run_editor(args, configuration, camera_modes, raw_directory, log_path)
        captures.extend(
            load_captures(
                raw_directory,
                configuration,
                counts,
                camera_modes,
                args.repeats,
                args.trim_frames,
            )
        )

    deltas = calculate_deltas(
        captures, configurations, counts, camera_modes, args.repeats
    )
    validate_results(captures, deltas, configurations, counts, camera_modes)

    write_csv(run_directory / "GpuStarfieldBenchmark.csv", deltas)
    report = write_markdown(
        run_directory / "GpuStarfieldBenchmark.md",
        deltas,
        configurations,
        counts,
        camera_modes,
    )
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
