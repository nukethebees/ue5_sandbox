from __future__ import annotations

import csv
import io
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

PERF_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PERF_ROOT))

from lib.benchmark_comparison import (
    CompareOptions,
    JsonObject,
    benchmark_arguments,
    compare_results,
    normalize_source_file,
    parse_options,
    parse_zone_csv,
    percentage_delta,
    print_summary,
)


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
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "zones.csv"
            write_csv(path, [["Tick", "native/sim.cpp", "20", "100", "50", "2", "50", "40", "60", "10"]])

            zones, warnings = parse_zone_csv(path)

        self.assertFalse(warnings)
        zone = next(iter(zones.values()))
        self.assertEqual(zone.name, "Tick")
        self.assertEqual(zone.count, 2)
        self.assertEqual(zone.total_ns, 100)

    def test_reports_ambiguous_semantic_identity_without_using_line(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
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
            skip_build=True,
            connection_timeout_seconds=1.0,
            process_timeout_seconds=1.0,
            top=5,
            runner_arguments=(),
        )

    def test_matches_zones_when_source_lines_move(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            for label, line, total in (("a", "20", "100"), ("b", "90", "120")):
                run = output / label
                run.mkdir()
                row = ["Tick", "native/sim.cpp", line, total, "50", "2", "50", "40", "60", "10"]
                write_csv(run / "inclusive.csv", [row])
                write_csv(run / "self.csv", [row])
            result = {"workload": {"completed_ticks": 2}, "timing": {"mean_tick_microseconds": 10.0}}

            comparison = compare_results(self.make_options(output), result, result)

        self.assertEqual(comparison["status"], "complete")
        self.assertEqual(len(comparison["zones"]), 1)
        self.assertAlmostEqual(comparison["zones"][0]["inclusive"]["delta_percent"], 20.0)

    def test_reports_invocation_count_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            for label, count in (("a", "2"), ("b", "3")):
                run = output / label
                run.mkdir()
                row = ["Tick", "native/sim.cpp", "20", "100", "50", count, "50", "40", "60", "10"]
                write_csv(run / "inclusive.csv", [row])
                write_csv(run / "self.csv", [row])
            result = {"workload": {"completed_ticks": 2}, "timing": {"mean_tick_microseconds": 10.0}}

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


if __name__ == "__main__":
    unittest.main()
