from __future__ import annotations

import csv
import io
import json
import os
import subprocess
import sys
import tempfile
import threading
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest import mock

PERF_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PERF_ROOT))
TEST_TEMP_ROOT = PERF_ROOT.parents[1] / ".local" / "tests" / "perf-comparison"
TEST_TEMP_ROOT.mkdir(parents=True, exist_ok=True)

from lib.benchmark_comparison import (
    CompareOptions,
    JsonObject,
    PROFILER_READY_MESSAGE,
    PipelineError,
    _monitor_benchmark_stderr,  # pyright: ignore[reportPrivateUsage]
    _stop_process,  # pyright: ignore[reportPrivateUsage]
    _wait_for_profiler_ready,  # pyright: ignore[reportPrivateUsage]
    benchmark_arguments,
    compare_results,
    main,
    normalize_source_file,
    parse_options,
    parse_zone_csv,
    percentage_delta,
    print_summary,
)
from lib import benchmark_comparison as comparison_module


HEADER = [
    "name",
    "src_file",
    "src_line",
    "total_ns",
    "total_perc",
    "counts",
    "mean_ns",
    "min_ns",
    "max_ns",
    "std_ns",
]


def write_csv(path: Path, rows: list[list[str]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.writer(output)
        writer.writerow(HEADER)
        writer.writerows(rows)


def temporary_directory() -> tempfile.TemporaryDirectory[str]:
    return tempfile.TemporaryDirectory(dir=TEST_TEMP_ROOT)


class CommandLineTests(unittest.TestCase):
    def test_parses_comparison_and_forwarded_arguments(self) -> None:
        options = parse_options(
            [
                "--level", __file__, "--seconds", "2.5", "--game-speed", "10",
                "--a-preset", "a", "--b-preset", "b", "--", "--telemetry",
            ]
        )

        self.assertEqual(options.seconds, 2.5)
        self.assertEqual(options.game_speed, 10)
        self.assertEqual(options.runner_arguments, ("--telemetry",))
        self.assertEqual(benchmark_arguments(options).count("--telemetry"), 1)
        self.assertIn("--wait-for-profiler", benchmark_arguments(options))

    def test_rejects_managed_forwarded_arguments(self) -> None:
        with self.assertRaises(SystemExit):
            parse_options(
                [
                    "--level", __file__, "--seconds", "1", "--a-preset", "a",
                    "--b-preset", "b", "--", "--seconds", "3",
                ]
            )

    def test_rejects_invalid_workload(self) -> None:
        with self.assertRaises(SystemExit):
            parse_options(
                [
                    "--level", __file__, "--seconds", "0", "--a-preset", "a",
                    "--b-preset", "b",
                ]
            )


class CsvTests(unittest.TestCase):
    def test_parses_zone_statistics(self) -> None:
        with temporary_directory() as directory:
            path = Path(directory) / "zones.csv"
            write_csv(path, [["Tick", "native/sim.cpp", "20", "100", "50", "2", "50", "40", "60", "10"]])

            zones, warnings = parse_zone_csv(path)

        self.assertFalse(warnings)
        zone = next(iter(zones.values()))
        self.assertEqual(zone.name, "Tick")
        self.assertEqual(zone.count, 2)
        self.assertEqual(zone.total_ns, 100)

    def test_reports_ambiguous_semantic_identity_without_using_line(self) -> None:
        with temporary_directory() as directory:
            path = Path(directory) / "zones.csv"
            write_csv(
                path,
                [
                    ["Tick", "native/sim.cpp", "20", "100", "50", "2", "50", "40", "60", "10"],
                    ["Tick", "native/sim.cpp", "40", "100", "50", "2", "50", "40", "60", "10"],
                ],
            )

            zones, warnings = parse_zone_csv(path)

        self.assertFalse(zones)
        self.assertEqual(warnings[0]["code"], "ambiguous_zone_identity")

    def test_normalizes_repository_source_paths(self) -> None:
        root = Path("C:/repo")
        self.assertEqual(normalize_source_file("C:\\repo\\native\\sim.cpp", root), "native/sim.cpp")


class ComparisonTests(unittest.TestCase):
    def make_options(self, output: Path) -> CompareOptions:
        return CompareOptions(
            level=Path(__file__),
            seconds=1.0,
            game_speed=1,
            a_preset="a",
            b_preset="b",
            output_dir=output,
            output_dir_explicit=True,
            skip_build=True,
            connection_timeout_seconds=1.0,
            process_timeout_seconds=1.0,
            top=5,
            runner_arguments=(),
        )

    def test_matches_zones_when_source_lines_move(self) -> None:
        with temporary_directory() as directory:
            output = Path(directory)
            for label, line, total in (("a", "20", "100"), ("b", "90", "120")):
                run = output / label
                run.mkdir()
                row = ["Tick", "native/sim.cpp", line, total, "50", "2", "50", "40", "60", "10"]
                write_csv(run / "inclusive.csv", [row])
                write_csv(run / "self.csv", [row])
            result = {
                "workload": {"completed_ticks": 2},
                "final_state": {"alive_entities": 1},
                "timing": {"mean_tick_microseconds": 10.0},
            }

            comparison = compare_results(self.make_options(output), result, result)

        self.assertEqual(comparison["status"], "complete")
        self.assertEqual(len(comparison["zones"]), 1)
        self.assertAlmostEqual(comparison["zones"][0]["inclusive"]["delta_percent"], 20.0)

    def test_reports_invocation_count_mismatch(self) -> None:
        with temporary_directory() as directory:
            output = Path(directory)
            for label, count in (("a", "2"), ("b", "3")):
                run = output / label
                run.mkdir()
                row = ["Tick", "native/sim.cpp", "20", "100", "50", count, "50", "40", "60", "10"]
                write_csv(run / "inclusive.csv", [row])
                write_csv(run / "self.csv", [row])
            result = {
                "workload": {"completed_ticks": 2},
                "final_state": {"alive_entities": 1},
                "timing": {"mean_tick_microseconds": 10.0},
            }

            comparison = compare_results(self.make_options(output), result, result)

        self.assertEqual(comparison["status"], "complete_with_warnings")
        self.assertFalse(comparison["zones"][0]["comparable"])
        self.assertIn("invocation_count_mismatch", {warning["code"] for warning in comparison["warnings"]})

    def test_percentage_delta_handles_zero_baseline(self) -> None:
        self.assertIsNone(percentage_delta(0, 1))
        self.assertEqual(percentage_delta(10, 12), 20.0)

    def test_summary_prints_tracy_comparison_paths(self) -> None:
        output = Path("results")
        benchmark_result = {"timing": {"mean_tick_microseconds": 10.0}}
        comparison: JsonObject = {
            "status": "complete",
            "overall": {
                "a": benchmark_result,
                "b": benchmark_result,
                "delta_percent": 0.0,
            },
            "zones": [],
        }
        standard_output = io.StringIO()

        with redirect_stdout(standard_output):
            print_summary(comparison, output, top=5)

        summary = standard_output.getvalue()
        self.assertIn(str(output / "a" / "capture.tracy"), summary)
        self.assertIn(str(output / "b" / "capture.tracy"), summary)
        self.assertIn("Compare > Open second trace", summary)

    def test_reports_final_state_mismatch(self) -> None:
        with temporary_directory() as directory:
            output = Path(directory)
            for label in ("a", "b"):
                run = output / label
                run.mkdir()
                row = ["Tick", "native/sim.cpp", "20", "100", "50", "2", "50", "40", "60", "10"]
                write_csv(run / "inclusive.csv", [row])
                write_csv(run / "self.csv", [row])
            a_result = {
                "workload": {"completed_ticks": 2},
                "final_state": {"alive_entities": 1},
                "timing": {"mean_tick_microseconds": 10.0},
            }
            b_result = {
                "workload": {"completed_ticks": 2},
                "final_state": {"alive_entities": 2},
                "timing": {"mean_tick_microseconds": 10.0},
            }

            comparison = compare_results(self.make_options(output), a_result, b_result)

        self.assertEqual(comparison["status"], "incomparable")
        self.assertIn("final_state_mismatch", {warning["code"] for warning in comparison["warnings"]})


class ProcessTests(unittest.TestCase):
    def test_waits_for_profiler_ready_and_cleans_up_process(self) -> None:
        process = subprocess.Popen(
            [
                sys.executable,
                "-c",
                (
                    "import sys,time; "
                    f"print({PROFILER_READY_MESSAGE!r}, file=sys.stderr, flush=True); "
                    "time.sleep(30)"
                ),
            ],
            stderr=subprocess.PIPE,
            text=True,
        )
        monitor: threading.Thread | None = None
        try:
            standard_error_stream = process.stderr
            assert standard_error_stream is not None
            profiler_ready = threading.Event()
            standard_error = io.StringIO()
            monitor = threading.Thread(
                target=_monitor_benchmark_stderr,
                args=(standard_error_stream, standard_error, profiler_ready),
                daemon=True,
            )
            monitor.start()

            _wait_for_profiler_ready(process, profiler_ready, timeout=2.0, label="a")

            self.assertIn(PROFILER_READY_MESSAGE, standard_error.getvalue())
            self.assertIsNone(process.poll())
        finally:
            _stop_process(process)
            if monitor is not None:
                monitor.join(timeout=2.0)
            if process.stderr is not None:
                process.stderr.close()
        self.assertIsNotNone(process.returncode)

    def test_reports_process_exit_before_profiler_ready(self) -> None:
        process = subprocess.Popen(
            [sys.executable, "-c", "raise SystemExit(7)"],
            stderr=subprocess.PIPE,
            text=True,
        )
        standard_error_stream = process.stderr
        assert standard_error_stream is not None
        profiler_ready = threading.Event()
        standard_error = io.StringIO()
        monitor = threading.Thread(
            target=_monitor_benchmark_stderr,
            args=(standard_error_stream, standard_error, profiler_ready),
            daemon=True,
        )
        monitor.start()

        with self.assertRaisesRegex(PipelineError, "before requesting a profiler"):
            _wait_for_profiler_ready(process, profiler_ready, timeout=2.0, label="a")

        monitor.join(timeout=2.0)
        standard_error_stream.close()
        self.assertEqual(process.returncode, 7)


class OutputDirectoryTests(unittest.TestCase):
    def test_rejects_nonempty_explicit_output_directory(self) -> None:
        with temporary_directory() as directory:
            output = Path(directory)
            sentinel = output / "keep.txt"
            sentinel.write_text("keep", encoding="utf-8")
            standard_error = io.StringIO()

            with redirect_stdout(io.StringIO()), redirect_stderr(standard_error):
                return_code = main(
                    [
                        "--level", __file__, "--seconds", "1", "--a-preset", "a",
                        "--b-preset", "b", "--output-dir", str(output), "--skip-build",
                    ]
                )

            self.assertEqual(return_code, 1)
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "keep")
            self.assertIn("output directory is not empty", standard_error.getvalue())

    def test_process_launch_error_writes_structured_failure(self) -> None:
        with temporary_directory() as directory:
            output = Path(directory) / "result"
            standard_error = io.StringIO()
            arguments = [
                "--level", __file__, "--seconds", "1", "--a-preset", "a",
                "--b-preset", "b", "--output-dir", str(output), "--skip-build", "--jobserver-child",
            ]

            with (
                mock.patch.dict(os.environ, {"NUKETHEBEES_JOBSERVER_JOB": "test"}),
                mock.patch.object(comparison_module, "run_comparison", side_effect=OSError("launch failed")),
                redirect_stderr(standard_error),
            ):
                return_code = main(arguments)

            comparison = json.loads((output / "comparison.json").read_text(encoding="utf-8"))

        self.assertEqual(return_code, 1)
        self.assertEqual(comparison["status"], "failed")
        self.assertEqual(comparison["warnings"][0]["code"], "operating_system_error")
        self.assertIn("launch failed", standard_error.getvalue())


class NestedJobserverTests(unittest.TestCase):
    def test_submission_rejection_writes_structured_failure(self) -> None:
        with temporary_directory() as directory:
            output = Path(directory) / "result"
            arguments = ["--level", __file__, "--seconds", "1", "--a-preset", "a",
                         "--b-preset", "b", "--output-dir", str(output), "--skip-build"]
            with (
                mock.patch.dict(os.environ, {"NUKETHEBEES_JOBSERVER_JOB": "parent"}),
                mock.patch.object(comparison_module, "invoke_with_benchmark_access",
                                  side_effect=PipelineError("jobserver_failed", "nested claim rejected")),
                mock.patch.object(comparison_module, "run_comparison") as run,
                redirect_stderr(io.StringIO()),
            ):
                self.assertEqual(main(arguments), 1)
                run.assert_not_called()
            failure = json.loads((output / "comparison.json").read_text(encoding="utf-8"))
            self.assertEqual(failure["warnings"][0]["code"], "jobserver_failed")

    def test_only_marked_child_bypasses_submission(self) -> None:
        for marked in (False, True):
            with temporary_directory() as directory:
                arguments = ["--level", __file__, "--seconds", "1", "--a-preset", "a",
                             "--b-preset", "b", "--output-dir", str(Path(directory) / "result"),
                             "--skip-build"]
                if marked:
                    arguments.append("--jobserver-child")
                with (
                    mock.patch.dict(os.environ, {"NUKETHEBEES_JOBSERVER_JOB": "parent"}),
                    mock.patch.object(comparison_module, "invoke_with_benchmark_access", return_value=0) as submit,
                    mock.patch.object(comparison_module, "run_comparison", return_value=0) as run,
                ):
                    self.assertEqual(main(arguments), 0)
                self.assertEqual(submit.call_count, 0 if marked else 1)
                self.assertEqual(run.call_count, 1 if marked else 0)

    def test_marker_without_parent_fails_closed(self) -> None:
        with temporary_directory() as directory:
            arguments = ["--level", __file__, "--seconds", "1", "--a-preset", "a",
                         "--b-preset", "b", "--output-dir", str(Path(directory) / "result"),
                         "--skip-build", "--jobserver-child"]
            with (
                mock.patch.dict(os.environ, {"NUKETHEBEES_JOBSERVER_JOB": ""}),
                mock.patch.object(comparison_module, "run_comparison") as run,
                redirect_stderr(io.StringIO()),
            ):
                self.assertEqual(main(arguments), 1)
                run.assert_not_called()

    def test_submission_marks_child_and_requests_exclusive_resources(self) -> None:
        with temporary_directory() as directory:
            options = parse_options(["--level", __file__, "--seconds", "1", "--a-preset", "a",
                                     "--b-preset", "b", "--output-dir", directory])
            with (
                mock.patch.object(comparison_module, "_require_file"),
                mock.patch.object(subprocess, "run", return_value=subprocess.CompletedProcess([], 0)) as run,
            ):
                comparison_module.invoke_with_benchmark_access(options)
            command = run.call_args.args[0]
            self.assertIn("--jobserver-child", command)
            self.assertIn("--skip-build", command)
            for resource in ("machine", "benchmark"):
                self.assertTrue(any(command[index:index + 2] == ["--exclusive", resource]
                                    for index in range(len(command) - 1)))


if __name__ == "__main__":
    unittest.main()
