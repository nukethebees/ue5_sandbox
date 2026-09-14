from __future__ import annotations

import argparse
import csv
import datetime
import json
import os
import re
import socket
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Any, TextIO, cast


ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "tools" / "perf" / "compare.py"
DEFAULT_OUTPUT_ROOT = ROOT / ".local" / "benchmarks" / "tracy-comparison"
BENCHMARK_EXECUTABLE = "native-simulation-benchmark.exe"
CAPTURE_EXECUTABLE = "tracy-capture.exe"
CSVEXPORT_EXECUTABLE = "tracy-csvexport.exe"
PROFILER_READY_MESSAGE = "native-simulation-benchmark: profiler-ready"
RESERVED_RUNNER_OPTIONS = {
    "--level",
    "--seconds",
    "--game-speed",
    "--wait-for-profiler",
}

JsonObject = dict[str, Any]
ZoneKey = tuple[str, str]


@dataclass(frozen=True)
class CompareOptions:
    level: Path
    seconds: float
    game_speed: int
    a_preset: str
    b_preset: str
    output_dir: Path
    output_dir_explicit: bool
    skip_build: bool
    connection_timeout_seconds: float
    process_timeout_seconds: float
    top: int
    runner_arguments: tuple[str, ...]


@dataclass(frozen=True)
class ZoneStatistics:
    name: str
    source_file: str
    source_line: int
    total_ns: int
    total_percent: float
    count: int
    mean_ns: int
    minimum_ns: int
    maximum_ns: int


class PipelineError(RuntimeError):
    def __init__(self, code: str, message: str) -> None:
        super().__init__(message)
        self.code = code


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Compare two native benchmark configurations using Tracy captures."
    )
    parser.add_argument("--level", type=Path, required=True)
    parser.add_argument("--seconds", type=float, required=True)
    parser.add_argument("--game-speed", type=int, default=1)
    parser.add_argument("--a-preset", required=True)
    parser.add_argument("--b-preset", required=True)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--connection-timeout-seconds", type=float, default=30.0)
    parser.add_argument("--process-timeout-seconds", type=float, default=1800.0)
    parser.add_argument("--top", type=int, default=5)
    parser.add_argument("runner_arguments", nargs=argparse.REMAINDER)
    return parser


def parse_options(arguments: list[str]) -> CompareOptions:
    parser = build_argument_parser()
    parsed = parser.parse_args(arguments)

    if not _positive_finite(parsed.seconds):
        parser.error("--seconds must be positive and finite")
    if parsed.game_speed <= 0:
        parser.error("--game-speed must be positive")
    if not _positive_finite(parsed.connection_timeout_seconds):
        parser.error("--connection-timeout-seconds must be positive and finite")
    if not _positive_finite(parsed.process_timeout_seconds):
        parser.error("--process-timeout-seconds must be positive and finite")
    if parsed.top <= 0:
        parser.error("--top must be positive")
    for preset in (parsed.a_preset, parsed.b_preset):
        if not re.fullmatch(r"[A-Za-z0-9_.-]+", preset):
            parser.error(f"invalid CMake preset name: {preset}")

    runner_arguments = tuple(parsed.runner_arguments)
    if runner_arguments and runner_arguments[0] == "--":
        runner_arguments = runner_arguments[1:]
    for argument in runner_arguments:
        option_name = argument.split("=", maxsplit=1)[0]
        if option_name in RESERVED_RUNNER_OPTIONS:
            parser.error(f"{option_name} is managed by the comparison tool")

    timestamp = datetime.datetime.now(datetime.UTC).strftime("%Y%m%d-%H%M%S-%fZ")
    output_dir = parsed.output_dir or DEFAULT_OUTPUT_ROOT / timestamp
    return CompareOptions(
        level=parsed.level.resolve(),
        seconds=parsed.seconds,
        game_speed=parsed.game_speed,
        a_preset=parsed.a_preset,
        b_preset=parsed.b_preset,
        output_dir=output_dir.resolve(),
        output_dir_explicit=parsed.output_dir is not None,
        skip_build=parsed.skip_build,
        connection_timeout_seconds=parsed.connection_timeout_seconds,
        process_timeout_seconds=parsed.process_timeout_seconds,
        top=parsed.top,
        runner_arguments=runner_arguments,
    )


def _positive_finite(value: float) -> bool:
    return value > 0.0 and value != float("inf") and value == value


def _format_number(value: float) -> str:
    return format(value, ".17g")


def benchmark_arguments(options: CompareOptions) -> list[str]:
    return [
        "--level",
        str(options.level),
        "--seconds",
        _format_number(options.seconds),
        "--game-speed",
        str(options.game_speed),
        *options.runner_arguments,
        "--wait-for-profiler",
        _format_number(options.connection_timeout_seconds),
    ]


def _options_arguments(options: CompareOptions) -> list[str]:
    arguments = [
        "--level",
        str(options.level),
        "--seconds",
        _format_number(options.seconds),
        "--game-speed",
        str(options.game_speed),
        "--a-preset",
        options.a_preset,
        "--b-preset",
        options.b_preset,
        "--output-dir",
        str(options.output_dir),
        "--connection-timeout-seconds",
        _format_number(options.connection_timeout_seconds),
        "--process-timeout-seconds",
        _format_number(options.process_timeout_seconds),
        "--top",
        str(options.top),
        "--skip-build",
    ]
    if options.runner_arguments:
        arguments.extend(("--", *options.runner_arguments))
    return arguments


def _binary_path(preset: str, executable: str) -> Path:
    return ROOT / "out" / "build" / preset / "bin" / executable


def _run_build(command: list[str]) -> None:
    print(f"> {' '.join(command)}", flush=True)
    result = subprocess.run(command, cwd=ROOT, check=False)
    if result.returncode != 0:
        raise PipelineError("build_failed", f"command exited with {result.returncode}: {' '.join(command)}")


def build_prerequisites(options: CompareOptions) -> None:
    _run_build(["cmake", "--preset", "tracy-tools"])
    _run_build(["cmake", "--build", "--preset", "tracy-tools"])
    for preset in dict.fromkeys((options.a_preset, options.b_preset)):
        _run_build(["cmake", "--preset", preset])
        _run_build(
            ["cmake", "--build", "--preset", preset, "--target", "native-simulation-benchmark"]
        )


def _git_output(arguments: list[str]) -> str:
    result = subprocess.run(
        ["git", *arguments], cwd=ROOT, capture_output=True, text=True, check=False
    )
    return result.stdout.strip() if result.returncode == 0 else "unknown"


def _tracy_version() -> str:
    version_header = ROOT / "native" / "third_party" / "tracy" / "public" / "common" / "TracyVersion.hpp"
    contents = version_header.read_text(encoding="utf-8")
    values = [re.search(rf"{name}\s*=\s*(\d+)", contents) for name in ("Major", "Minor", "Patch")]
    if any(value is None for value in values):
        return "unknown"
    return ".".join(value.group(1) for value in values if value is not None)


def make_manifest(options: CompareOptions, status: str) -> JsonObject:
    return {
        "schema_version": 1,
        "created_utc": datetime.datetime.now(datetime.UTC).isoformat(),
        "status": status,
        "benchmark": {
            "arguments": {
                "level": str(options.level),
                "seconds": options.seconds,
                "game_speed": options.game_speed,
                "additional": list(options.runner_arguments),
            },
            "effective_argv": benchmark_arguments(options),
        },
        "source": {
            "git_sha": _git_output(["rev-parse", "HEAD"]),
            "dirty": bool(_git_output(["status", "--porcelain"])),
        },
        "tracy": {
            "version": _tracy_version(),
            "git_sha": _git_output(["-C", "native/third_party/tracy", "rev-parse", "HEAD"]),
            "capture_executable": str(_binary_path("tracy-tools", CAPTURE_EXECUTABLE)),
            "csvexport_executable": str(_binary_path("tracy-tools", CSVEXPORT_EXECUTABLE)),
        },
        "configurations": {
            "a": {
                "preset": options.a_preset,
                "benchmark_executable": str(_binary_path(options.a_preset, BENCHMARK_EXECUTABLE)),
            },
            "b": {
                "preset": options.b_preset,
                "benchmark_executable": str(_binary_path(options.b_preset, BENCHMARK_EXECUTABLE)),
            },
        },
    }


def write_json(path: Path, value: JsonObject) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def write_failure(options: CompareOptions, manifest: JsonObject, error: PipelineError) -> None:
    warning = {"code": error.code, "severity": "error", "message": str(error)}
    manifest["status"] = "failed"
    manifest["warnings"] = [warning]
    write_json(options.output_dir / "manifest.json", manifest)
    write_json(
        options.output_dir / "comparison.json",
        {"schema_version": 1, "status": "failed", "overall": None, "zones": [], "warnings": [warning]},
    )


def _require_file(path: Path, description: str) -> None:
    if not path.is_file():
        raise PipelineError("missing_executable", f"{description} was not built: {path}")


def invoke_with_benchmark_access(options: CompareOptions) -> int:
    local_app_data = os.environ.get("LOCALAPPDATA")
    if not local_app_data:
        raise PipelineError("jobserver_unavailable", "LOCALAPPDATA is not set")
    jobserver = Path(local_app_data) / "NukeTheBees" / "jobserver" / "bin" / "jobserver.exe"
    _require_file(jobserver, "jobserver")
    command = [
        str(jobserver),
        "run",
        "--name",
        "Tracy native benchmark comparison",
        "--kind",
        "benchmark",
        "--worktree",
        str(ROOT),
        "--exclusive",
        "machine",
        "--exclusive",
        "benchmark",
        "--",
        sys.executable,
        str(SCRIPT),
        *_options_arguments(replace(options, skip_build=True)),
    ]
    return_code = subprocess.run(command, cwd=ROOT, check=False).returncode
    if return_code != 0 and not (options.output_dir / "comparison.json").is_file():
        raise PipelineError("jobserver_failed", f"jobserver command exited with {return_code}")
    return return_code


def _unused_loopback_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(("127.0.0.1", 0))
        return int(listener.getsockname()[1])


def _stop_process(process: subprocess.Popen[str] | None) -> None:
    if process is None or process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=5.0)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5.0)


def _monitor_benchmark_stderr(
    stream: TextIO, destination: TextIO, profiler_ready: threading.Event
) -> None:
    for line in stream:
        destination.write(line)
        destination.flush()
        if line.rstrip("\r\n") == PROFILER_READY_MESSAGE:
            profiler_ready.set()


def _wait_for_profiler_ready(
    process: subprocess.Popen[str], profiler_ready: threading.Event, timeout: float, label: str
) -> None:
    deadline = time.monotonic() + timeout
    while not profiler_ready.wait(timeout=0.05):
        return_code = process.poll()
        if return_code is not None:
            raise PipelineError(
                "benchmark_failed",
                f"benchmark {label} exited with {return_code} before requesting a profiler",
            )
        if time.monotonic() >= deadline:
            raise PipelineError(
                "benchmark_timeout", f"benchmark {label} did not request a profiler in time"
            )


def capture_run(options: CompareOptions, label: str, preset: str) -> JsonObject:
    output = options.output_dir / label
    output.mkdir(parents=True, exist_ok=True)
    capture_path = output / "capture.tracy"
    capture_log_path = output / "capture.log"
    benchmark_error_path = output / "benchmark.stderr.log"
    capture_executable = _binary_path("tracy-tools", CAPTURE_EXECUTABLE)
    benchmark_executable = _binary_path(preset, BENCHMARK_EXECUTABLE)
    port = _unused_loopback_port()
    environment = dict(os.environ)
    environment["TRACY_PORT"] = str(port)
    environment["TRACY_NO_EXIT"] = "1"

    capture_process: subprocess.Popen[str] | None = None
    benchmark_process: subprocess.Popen[str] | None = None
    stderr_thread: threading.Thread | None = None
    try:
        with capture_log_path.open("w", encoding="utf-8") as capture_log, benchmark_error_path.open(
            "w", encoding="utf-8"
        ) as benchmark_error:
            benchmark_process = subprocess.Popen(
                [str(benchmark_executable), *benchmark_arguments(options)],
                cwd=ROOT,
                env=environment,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            if benchmark_process.stderr is None:
                raise PipelineError("benchmark_failed", f"benchmark {label} stderr is unavailable")
            profiler_ready = threading.Event()
            stderr_thread = threading.Thread(
                target=_monitor_benchmark_stderr,
                args=(benchmark_process.stderr, benchmark_error, profiler_ready),
                daemon=True,
            )
            stderr_thread.start()
            _wait_for_profiler_ready(
                benchmark_process, profiler_ready, options.process_timeout_seconds, label
            )

            capture_process = subprocess.Popen(
                [str(capture_executable), "-o", str(capture_path), "-a", "127.0.0.1", "-p", str(port)],
                cwd=ROOT,
                stdout=capture_log,
                stderr=subprocess.STDOUT,
                text=True,
            )
            try:
                benchmark_return_code = benchmark_process.wait(
                    timeout=options.process_timeout_seconds
                )
            except subprocess.TimeoutExpired as error:
                raise PipelineError("benchmark_timeout", f"benchmark {label} exceeded the process timeout") from error
            if benchmark_process.stdout is None:
                raise PipelineError("benchmark_failed", f"benchmark {label} stdout is unavailable")
            standard_output = benchmark_process.stdout.read()
            if benchmark_return_code != 0:
                capture_return_code = capture_process.poll()
                if capture_return_code not in (None, 0):
                    raise PipelineError(
                        "capture_failed", f"Tracy capture {label} exited with {capture_return_code}"
                    )
                raise PipelineError(
                    "benchmark_failed", f"benchmark {label} exited with {benchmark_return_code}"
                )
            try:
                decoded = json.loads(standard_output.strip())
            except json.JSONDecodeError as error:
                raise PipelineError("benchmark_result_invalid", f"benchmark {label} did not emit one JSON result") from error
            if not isinstance(decoded, dict):
                raise PipelineError("benchmark_result_invalid", f"benchmark {label} result was not an object")
            result = cast(JsonObject, decoded)
            environment_data = result.get("environment")
            if not isinstance(environment_data, dict):
                raise PipelineError("benchmark_result_invalid", f"benchmark {label} environment is missing")
            environment = cast(JsonObject, environment_data)
            if not bool(environment.get("tracy_enabled")):
                raise PipelineError("tracy_not_enabled", f"benchmark preset {preset} does not enable Tracy")
            write_json(output / "benchmark-result.json", result)

            try:
                capture_return_code = capture_process.wait(timeout=min(options.process_timeout_seconds, 120.0))
            except subprocess.TimeoutExpired as error:
                raise PipelineError("capture_timeout", f"Tracy capture {label} did not finish") from error
            if capture_return_code != 0:
                raise PipelineError("capture_failed", f"Tracy capture {label} exited with {capture_return_code}")
    finally:
        _stop_process(benchmark_process)
        _stop_process(capture_process)
        if stderr_thread is not None:
            stderr_thread.join(timeout=5.0)
        if benchmark_process is not None:
            if benchmark_process.stdout is not None:
                benchmark_process.stdout.close()
            if benchmark_process.stderr is not None:
                benchmark_process.stderr.close()

    if not capture_path.is_file() or capture_path.stat().st_size == 0:
        raise PipelineError("capture_failed", f"Tracy capture {label} was not created")
    return result


def export_capture(options: CompareOptions, label: str, self_time: bool) -> Path:
    output = options.output_dir / label
    destination = output / ("self.csv" if self_time else "inclusive.csv")
    command = [str(_binary_path("tracy-tools", CSVEXPORT_EXECUTABLE))]
    if self_time:
        command.append("--self")
    command.append(str(output / "capture.tracy"))
    try:
        result = subprocess.run(
            command,
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
            timeout=options.process_timeout_seconds,
        )
    except subprocess.TimeoutExpired as error:
        raise PipelineError("export_timeout", f"Tracy export {label} timed out") from error
    destination.write_text(result.stdout, encoding="utf-8")
    if result.returncode != 0:
        raise PipelineError("export_failed", f"Tracy export {label} failed: {result.stderr.strip()}")
    return destination


def normalize_source_file(source_file: str, root: Path = ROOT) -> str:
    normalized = source_file.replace("\\", "/")
    root_text = root.resolve().as_posix().rstrip("/")
    if normalized.casefold().startswith((root_text + "/").casefold()):
        normalized = normalized[len(root_text) + 1 :]
    return normalized


def parse_zone_csv(path: Path) -> tuple[dict[ZoneKey, ZoneStatistics], list[JsonObject]]:
    zones: dict[ZoneKey, ZoneStatistics] = {}
    ambiguous: set[ZoneKey] = set()
    warnings: list[JsonObject] = []
    with path.open("r", encoding="utf-8-sig", newline="") as input_file:
        reader = csv.DictReader(input_file)
        required = {
            "name", "src_file", "src_line", "total_ns", "total_perc", "counts",
            "mean_ns", "min_ns", "max_ns", "std_ns",
        }
        if reader.fieldnames is None or not required.issubset(reader.fieldnames):
            raise PipelineError("csv_parse_failed", f"unexpected Tracy CSV header in {path}")
        for row in reader:
            if None in row:
                raise PipelineError("csv_parse_failed", f"malformed Tracy CSV row in {path}")
            try:
                source_file = normalize_source_file(row["src_file"])
                statistics = ZoneStatistics(
                    name=row["name"],
                    source_file=source_file,
                    source_line=int(row["src_line"]),
                    total_ns=int(row["total_ns"]),
                    total_percent=float(row["total_perc"]),
                    count=int(row["counts"]),
                    mean_ns=int(row["mean_ns"]),
                    minimum_ns=int(row["min_ns"]),
                    maximum_ns=int(row["max_ns"]),
                )
            except (KeyError, TypeError, ValueError) as error:
                raise PipelineError("csv_parse_failed", f"invalid Tracy CSV value in {path}") from error
            key = (statistics.name, statistics.source_file.casefold())
            if key in zones or key in ambiguous:
                zones.pop(key, None)
                ambiguous.add(key)
                warnings.append(
                    {
                        "code": "ambiguous_zone_identity",
                        "severity": "warning",
                        "message": f"multiple Tracy rows share identity {statistics.name} in {source_file}",
                        "zone": {"name": statistics.name, "source_file": source_file},
                    }
                )
            else:
                zones[key] = statistics
    return zones, warnings


def _statistics_json(statistics: ZoneStatistics | None) -> JsonObject | None:
    if statistics is None:
        return None
    return {
        "total_ns": statistics.total_ns,
        "total_percent": statistics.total_percent,
        "mean_ns": statistics.mean_ns,
        "min_ns": statistics.minimum_ns,
        "max_ns": statistics.maximum_ns,
        "source_line": statistics.source_line,
    }


def percentage_delta(a: int | float, b: int | float) -> float | None:
    return None if a == 0 else 100.0 * (b - a) / a


def compare_results(options: CompareOptions, a_result: JsonObject, b_result: JsonObject) -> JsonObject:
    warnings: list[JsonObject] = []
    parsed: dict[str, dict[str, dict[ZoneKey, ZoneStatistics]]] = {}
    for label in ("a", "b"):
        inclusive, inclusive_warnings = parse_zone_csv(options.output_dir / label / "inclusive.csv")
        self_times, self_warnings = parse_zone_csv(options.output_dir / label / "self.csv")
        warnings.extend(inclusive_warnings)
        warnings.extend(self_warnings)
        parsed[label] = {"inclusive": inclusive, "self": self_times}

    a_workload = a_result.get("workload")
    b_workload = b_result.get("workload")
    if a_workload != b_workload:
        warnings.append(
            {
                "code": "workload_mismatch",
                "severity": "error",
                "message": "benchmark results describe different deterministic workloads",
            }
        )

    a_final_state = a_result.get("final_state")
    b_final_state = b_result.get("final_state")
    if not isinstance(a_final_state, dict) or not isinstance(b_final_state, dict):
        raise PipelineError("benchmark_result_invalid", "benchmark final state is missing")
    if a_final_state != b_final_state:
        warnings.append(
            {
                "code": "final_state_mismatch",
                "severity": "error",
                "message": "benchmark results describe different final simulation states",
                "a": a_final_state,
                "b": b_final_state,
            }
        )

    keys = set(parsed["a"]["inclusive"]) | set(parsed["b"]["inclusive"])
    zones: list[JsonObject] = []
    for key in sorted(keys):
        a_inclusive = parsed["a"]["inclusive"].get(key)
        b_inclusive = parsed["b"]["inclusive"].get(key)
        a_self = parsed["a"]["self"].get(key)
        b_self = parsed["b"]["self"].get(key)
        present = a_inclusive is not None and b_inclusive is not None and a_self is not None and b_self is not None
        representative = a_inclusive or b_inclusive or a_self or b_self
        if representative is None:
            continue
        if not present:
            missing = "a" if a_inclusive is None or a_self is None else "b"
            warnings.append(
                {
                    "code": f"missing_zone_{missing}",
                    "severity": "warning",
                    "message": f"zone is missing from configuration {missing.upper()}",
                    "zone": {"name": representative.name, "source_file": representative.source_file},
                }
            )
        a_count = a_inclusive.count if a_inclusive is not None else None
        b_count = b_inclusive.count if b_inclusive is not None else None
        count_match = a_count is not None and a_count == b_count
        if present and not count_match:
            warnings.append(
                {
                    "code": "invocation_count_mismatch",
                    "severity": "warning",
                    "message": f"zone invocation count differs: A={a_count}, B={b_count}",
                    "zone": {"name": representative.name, "source_file": representative.source_file},
                }
            )
        zones.append(
            {
                "identity": {"name": representative.name, "source_file": representative.source_file},
                "invocation_count": {"a": a_count, "b": b_count, "match": count_match},
                "inclusive": {
                    "a": _statistics_json(a_inclusive),
                    "b": _statistics_json(b_inclusive),
                    "delta_percent": (
                        percentage_delta(a_inclusive.total_ns, b_inclusive.total_ns)
                        if a_inclusive is not None and b_inclusive is not None else None
                    ),
                },
                "self": {
                    "a": _statistics_json(a_self),
                    "b": _statistics_json(b_self),
                    "delta_percent": (
                        percentage_delta(a_self.total_ns, b_self.total_ns)
                        if a_self is not None and b_self is not None else None
                    ),
                },
                "comparable": present and count_match,
            }
        )

    try:
        a_metric = float(a_result["timing"]["mean_tick_microseconds"])
        b_metric = float(b_result["timing"]["mean_tick_microseconds"])
    except (KeyError, TypeError, ValueError) as error:
        raise PipelineError("benchmark_result_invalid", "benchmark timing result is missing") from error
    status = "incomparable" if any(warning["severity"] == "error" for warning in warnings) else (
        "complete_with_warnings" if warnings else "complete"
    )
    return {
        "schema_version": 1,
        "status": status,
        "overall": {
            "metric": "mean_tick_microseconds",
            "a": a_result,
            "b": b_result,
            "delta_percent": percentage_delta(a_metric, b_metric),
        },
        "zones": zones,
        "warnings": warnings,
    }


def print_summary(comparison: JsonObject, output_dir: Path, top: int) -> None:
    overall = comparison["overall"]
    a_value = float(overall["a"]["timing"]["mean_tick_microseconds"])
    b_value = float(overall["b"]["timing"]["mean_tick_microseconds"])
    delta = overall["delta_percent"]
    comparable = [zone for zone in comparison["zones"] if zone["comparable"]]
    mismatched = len(comparison["zones"]) - len(comparable)
    print(f"Tracy comparison: {comparison['status']}")
    print(f"Overall mean tick: A {a_value:.3f} us, B {b_value:.3f} us ({delta:+.2f}%)")
    print(f"Zones: {len(comparable)} comparable, {mismatched} mismatched")

    differences = sorted(
        comparable,
        key=lambda zone: zone["inclusive"]["b"]["total_ns"] - zone["inclusive"]["a"]["total_ns"],
        reverse=True,
    )
    regressions = [zone for zone in differences if zone["inclusive"]["delta_percent"] > 0][:top]
    improvements = [zone for zone in reversed(differences) if zone["inclusive"]["delta_percent"] < 0][:top]
    for heading, selected in (("Largest regressions", regressions), ("Largest improvements", improvements)):
        if not selected:
            continue
        print(f"{heading}:")
        for zone in selected:
            delta_ns = zone["inclusive"]["b"]["total_ns"] - zone["inclusive"]["a"]["total_ns"]
            print(
                f"  {zone['inclusive']['delta_percent']:+8.2f}% {delta_ns / 1_000_000:+10.3f} ms  "
                f"{zone['identity']['name']}"
            )
    print(f"Results: {output_dir}")
    print(f"Tracy capture A: {output_dir / 'a' / 'capture.tracy'}")
    print(f"Tracy capture B: {output_dir / 'b' / 'capture.tracy'}")
    print("Open capture A in tracy-profiler, then use Compare > Open second trace and select capture B.")


def run_comparison(options: CompareOptions, manifest: JsonObject) -> int:
    capture_executable = _binary_path("tracy-tools", CAPTURE_EXECUTABLE)
    csvexport_executable = _binary_path("tracy-tools", CSVEXPORT_EXECUTABLE)
    _require_file(options.level, "benchmark level")
    _require_file(capture_executable, "tracy-capture")
    _require_file(csvexport_executable, "tracy-csvexport")
    _require_file(_binary_path(options.a_preset, BENCHMARK_EXECUTABLE), "benchmark A")
    _require_file(_binary_path(options.b_preset, BENCHMARK_EXECUTABLE), "benchmark B")

    a_result = capture_run(options, "a", options.a_preset)
    b_result = capture_run(options, "b", options.b_preset)
    for label in ("a", "b"):
        export_capture(options, label, self_time=False)
        export_capture(options, label, self_time=True)
    comparison = compare_results(options, a_result, b_result)
    write_json(options.output_dir / "comparison.json", comparison)
    manifest["status"] = comparison["status"]
    manifest["warnings"] = comparison["warnings"]
    write_json(options.output_dir / "manifest.json", manifest)
    print_summary(comparison, options.output_dir, options.top)
    return 2 if comparison["status"] == "incomparable" else 0


def main(arguments: list[str]) -> int:
    options = parse_options(arguments)
    inside_jobserver = bool(os.environ.get("NUKETHEBEES_JOBSERVER_JOB"))
    if (
        options.output_dir_explicit
        and not inside_jobserver
        and options.output_dir.exists()
        and any(options.output_dir.iterdir())
    ):
        print(f"Tracy comparison failed: output directory is not empty: {options.output_dir}", file=sys.stderr)
        return 1
    options.output_dir.mkdir(parents=True, exist_ok=True)
    manifest = make_manifest(options, "preparing")
    write_json(options.output_dir / "manifest.json", manifest)

    try:
        if not options.skip_build:
            build_prerequisites(options)
        if not inside_jobserver:
            return invoke_with_benchmark_access(options)
        return run_comparison(options, manifest)
    except PipelineError as error:
        write_failure(options, manifest, error)
        print(f"Tracy comparison failed: {error}", file=sys.stderr)
        print(f"Results: {options.output_dir}", file=sys.stderr)
        return 1
    except OSError as error:
        pipeline_error = PipelineError("operating_system_error", str(error))
        write_failure(options, manifest, pipeline_error)
        print(f"Tracy comparison failed: {pipeline_error}", file=sys.stderr)
        print(f"Results: {options.output_dir}", file=sys.stderr)
        return 1
