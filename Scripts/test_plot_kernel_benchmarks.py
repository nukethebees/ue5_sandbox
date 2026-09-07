from __future__ import annotations

import argparse
import importlib.util
import json
import subprocess
import sys
import unittest
import uuid
from pathlib import Path
from types import ModuleType
from typing import Any


SCRIPT_PATH = Path(__file__).with_name("plot-kernel-benchmarks.py")
FIXTURE_PATH = Path(__file__).parent / "test-data" / "kernel-benchmark-results.json"


def load_plot_module() -> ModuleType:
    spec = importlib.util.spec_from_file_location("plot_kernel_benchmarks", SCRIPT_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load {SCRIPT_PATH}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


plotter = load_plot_module()
temporary_root = Path.cwd() / "out" / "test-temp" / "kernel-benchmark-plots"


def make_temporary_directory() -> Path:
    path = temporary_root / str(uuid.uuid4())
    path.mkdir(parents=True)
    return path


def benchmark_entry(
    real_time: float,
    *,
    run_name: str = "add_scaled/elementwise/flat/autovec-avx2/ordinary/aligned/32/real_time",
    time_unit: str = "ns",
    run_type: str = "iteration",
    aggregate_name: str | None = None,
) -> dict[str, Any]:
    entry: dict[str, Any] = {
        "name": run_name,
        "run_name": run_name,
        "run_type": run_type,
        "real_time": real_time,
        "time_unit": time_unit,
    }
    if aggregate_name is not None:
        entry["aggregate_name"] = aggregate_name
        entry["aggregate_unit"] = "time"
    return entry


class BenchmarkLoadingTests(unittest.TestCase):
    def write_document(self, entries: list[dict[str, Any]]) -> Path:
        path = make_temporary_directory() / "results.json"
        path.write_text(json.dumps({"benchmarks": entries}), encoding="utf-8")
        return path

    def test_raw_repetitions_use_median_and_sample_standard_deviation(self) -> None:
        path = self.write_document(
            [benchmark_entry(value) for value in (3.0, 1.0, 2.0)]
        )

        loaded = plotter.load_benchmarks(path)

        self.assertEqual(len(loaded.points), 1)
        self.assertEqual(loaded.points[0].key.layout, "flat")
        self.assertEqual(loaded.points[0].key.policy, "elementwise")
        self.assertEqual(loaded.points[0].median_time_ns, 2.0)
        self.assertEqual(loaded.points[0].time_stddev_ns, 1.0)
        self.assertEqual(loaded.points[0].ns_per_element, 0.0625)
        self.assertEqual(loaded.points[0].giga_elements_per_second, 16.0)

    def test_aggregate_only_results_and_time_units_are_supported(self) -> None:
        path = self.write_document(
            [
                benchmark_entry(
                    0.002,
                    time_unit="us",
                    run_type="aggregate",
                    aggregate_name="median",
                ),
                benchmark_entry(
                    0.0005,
                    time_unit="us",
                    run_type="aggregate",
                    aggregate_name="stddev",
                ),
            ]
        )

        loaded = plotter.load_benchmarks(path)

        self.assertEqual(loaded.points[0].median_time_ns, 2.0)
        self.assertEqual(loaded.points[0].time_stddev_ns, 0.5)

    def test_failed_backend_is_reported_without_discarding_successes(self) -> None:
        failed = benchmark_entry(1.0)
        failed["run_name"] = (
            "add_scaled/elementwise/flat/avx512/ordinary/aligned/32/real_time"
        )
        failed["error_occurred"] = True
        failed["error_message"] = "AVX-512 unavailable"
        path = self.write_document([benchmark_entry(2.0), failed])

        loaded = plotter.load_benchmarks(path)

        self.assertEqual(len(loaded.points), 1)
        self.assertEqual(len(loaded.skipped), 1)
        self.assertIn("AVX-512 unavailable", loaded.skipped[0])

    def test_invalid_benchmark_name_is_rejected(self) -> None:
        path = self.write_document([benchmark_entry(1.0, run_name="not/a/kernel")])

        with self.assertRaisesRegex(ValueError, "run_name must be"):
            plotter.load_benchmarks(path)


class PlotSmokeTests(unittest.TestCase):
    uv_executable: Path | None = None

    def test_locked_script_writes_headless_png_plots(self) -> None:
        if self.uv_executable is None:
            self.skipTest("uv executable was not supplied")

        output_directory = make_temporary_directory() / "plots"
        result = subprocess.run(
            [
                str(self.uv_executable),
                "run",
                "--locked",
                str(SCRIPT_PATH),
                str(FIXTURE_PATH),
                f"--output={output_directory}",
            ],
            capture_output=True,
            check=False,
            encoding="utf-8",
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        output_paths = sorted(output_directory.glob("*.png"))
        self.assertEqual(len(output_paths), 7)
        for output_path in output_paths:
            self.assertTrue(output_path.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"))


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--uv", type=Path)
    parser.add_argument("--temporary-root", type=Path)
    return parser.parse_args()


if __name__ == "__main__":
    arguments = parse_arguments()
    PlotSmokeTests.uv_executable = arguments.uv
    if arguments.temporary_root is not None:
        temporary_root = arguments.temporary_root
    unittest.main(argv=[sys.argv[0]])
